#include <signal.h>
#include <sys/types.h>

#include "daemon.h"

Daemon *Daemon::_self = nullptr;

Daemon &Daemon::instance() {
    if ( !_self )
        throw std::runtime_error( "daemon is not instanciated" );
    return *_self;
}

bool Daemon::hasInstance() {
    return _self;
}

void Daemon::run() {
    NOTE();

    while ( !_quit ) {
        std::vector< Socket > sockets;
        if ( _childPid != NoChild )
            sockets.push_back( _rope );
        try {
            loop( sockets );
        } catch ( brick::net::NetException &e ) {
            Logger::log( id(), std::string( "got a network exception: " ) + e.what() );
            reset();
        } catch ( std::exception &e ) {
            Logger::log( id(), std::string( "got an exception: " ) + e.what() );
            reset();
        } catch ( ... ) {
            Logger::log( id(), "got an unhandled exception" );
            reset();
        }
    }
}


bool Daemon::processControl( Message &message, Socket socket ) {
    Code code = message.tag< Code >();

    switch ( code ) {
    case Code::ENSLAVE:
        return enslave( message, std::move( socket ) );
    case Code::PEERS:
        startGrouping( message, std::move( socket ) );
        break;
    case Code::CONNECT:
        return connecting( message, std::move( socket ) );
    case Code::JOIN:
        return join( message, std::move( socket ) );
    case Code::GROUPED:
        return grouped( std::move( socket ) );
    case Code::PREPARE:
        prepare( std::move( socket ) );
        break;
    case Code::LEAVE:
        leave( std::move( socket ) );
        return false;
    case Code::DISCONNECT:
        release( std::move( socket ) );
        return false;
    case Code::CUTROPE:
        cutRope( std::move( socket ) );
        return false;
    case Code::SHUTDOWN:
        shutdown( std::move( socket ) );
        return false;
    case Code::ERROR:
        error( std::move( socket ) );
        return false;
    case Code::RENEGADE:
        renegade( message );
        return false;
    case Code::TABLE:
        table();
        break;
    case Code::INITDATA:
        initData( message );
        break;
    case Code::RUN:
        runMain( message, std::move( socket ) );
        return false;
    default:
        Logger::log( id(), std::string( "invalid command, got " ) + codeToString( code ) );
        break;
    }
    return true;
}

void Daemon::processDisconnected( Socket dead ) {
    NOTE();

    switch ( _state ) {
    case State::Leaving:
        break;
    case State::OverWatching:
        if ( _rope == dead )
            Logger::log( id(), "working child died" );
        else
            Logger::log( id(), "internal error happened - other connection than rope to the child has died" );
        setDefault();
        break;
    default:
        Logger::log( id(), "closed connection to " + info( dead ) );
        throw NetworkException{ "connection closed" };
    }
}

void Daemon::processIncomming( Socket incomming ) {
    readSocket( std::move( incomming ) );
}

bool Daemon::enslave( Message &message, Socket socket ) {
    NOTE();

    Message response( MessageType::Control );
    if ( _state != State::Free ) {
        response.tag( Code::REFUSE );
        socket->send( response );
        socket->close();
        return true;
    }

    int i;
    std::string selfName;

    response.tag( Code::ACK );
    socket->send( response );

    message.storeTo()
        >> i
        >> selfName;

    id( i );
    name( selfName );

    socket->properties( 0, "master", net().peerAddress( *socket ) );
    connections().lockedInsert( 0, std::move( socket ) ); // zero for master
    _state = State::Enslaved;
    return false;
}

void Daemon::startGrouping( Message &message, Socket socket ) {
    NOTE();

    Message response( MessageType::Control );
    int parameter;

    message.storeTo()
        >> parameter;

    if ( _state != State::Enslaved ) {
        response.tag( Code::REFUSE );
        socket->send( response );
        return;
    }
    response.tag( Code::ACK );
    socket->send( response );

    _state = State::FormingGroup;
    worldSize( parameter );
}

bool Daemon::connecting( Message &message, Socket socket ) {
    NOTE();

    Message response( MessageType::Control );
    int peerId;
    std::string peerName;
    Address peerAddress;

    message.storeTo()
        >> peerId
        >> peerName
        >> peerAddress;

    if ( _state != State::FormingGroup ) {
        response.tag( Code::REFUSE );
        socket->send( response );
        return true;
    }

    Socket peerSocket = connect( peerAddress );

    std::string name( this->name() );
    int id = this->id();

    message.reset();
    message.tag( Code::JOIN );
    message
        << id
        << name;
    peerSocket->send( message );

    peerSocket->receive( response );
    if ( response.tag< Code >() == Code::ACK ) {
        response.tag( Code::SUCCESS );
        socket->send( response );

        peerSocket->properties( peerId, peerName, net().peerAddress( *peerSocket ) );
        connections().lockedInsert( peerId, std::move( peerSocket ) );
        return false;
    }
    if ( response.tag< Code >() == Code::REFUSE ) {
        response.tag( Code::FAILED );
        socket->send( response );

        return true;
    }
    Logger::log( id, std::string( "invalid response, expected ACK or REFUSE, got " ) + codeToString( response.tag< Code >() ) );
    reset();
    return false;
}

bool Daemon::join( Message &message, Socket socket ) {
    NOTE();

    Message response( MessageType::Control );
    int peerId;
    std::string peerName;

    message.storeTo()
        >> peerId
        >> peerName;

    if ( _state != State::FormingGroup ) {
        response.tag( Code::REFUSE );
        socket->send( response );
        return true;
    }

    response.tag( Code::ACK );
    socket->send( response );

    socket->properties( peerId, peerName, net().peerAddress( *socket ) );
    connections().lockedInsert( peerId, std::move( socket ) );
    return false;
}

bool Daemon::grouped( Socket socket ) {
    NOTE();
    Message response( MessageType::Control );
    if ( _state != State::FormingGroup ) {
        response.tag( Code::REFUSE );
        socket->send( response );
        return true;
    }

    Socket top, bottom;
    std::tie( top, bottom ) = Communicator::socketPair();
    top->shutdown( brick::net::Shutdown::Write );
    bottom->shutdown( brick::net::Shutdown::Read );

    response.tag( Code::ACK );
    socket->send( response );

    int pid = ::fork();
    if ( pid == -1 )
        throw brick::net::SystemException( "fork" );
    if ( pid == 0 ) {
        becomeChild( std::move( bottom ) );
        return true;
    }
    else {
        _childPid = pid;
        becomeParent( std::move( top ) );
        return false;
    }
}

void Daemon::prepare( Socket socket ) {
    NOTE();
    Message response( MessageType::Control );
    if ( _state != State::Grouped && _state != State::FormingGroup ) {
        response.tag( Code::REFUSE );
        socket->send( response );
        return;
    }
    response.tag( Code::ACK );
    socket->send( response );
    _state = State::Leaving;
}

void Daemon::leave( Socket socket ) {
    NOTE();

    Message response( MessageType::Control );
    if ( _state != State::Leaving ) {
        response.tag( Code::REFUSE );
        socket->send( response );
        return;
    }

    response.tag( Code::ACK );
    socket->send( response );

    response.tag( Code::CUTROPE );
    _rope->send( response );

    _state = State::Enslaved;
    worldSize( 0 );
    _quit = true;
}

void Daemon::release( Socket socket ) {
    NOTE();
    Message response( MessageType::Control );
    if ( _state != State::Enslaved ) {
        response.tag( Code::REFUSE );
        socket->send( response );
        return;
    }
    response.tag( Code::ACK );
    socket->send( response );

    setDefault();
    return;
}

void Daemon::cutRope( Socket socket ) {
    NOTE();
    if ( _state != State::OverWatching || socket != _rope ) {
        Logger::log( id(), "command CUTROPE came from bad source or in a bad moment" );
        return;
    }
    setDefault();
    return;
}

void Daemon::shutdown( Socket socket ) {
    NOTE();
    Message response( MessageType::Control );
    if ( _state != State::Free ) {
        response.tag( Code::REFUSE );
        socket->send( response );
        return;
    }
    response.tag( Code::ACK );
    socket->send( response );
    _quit = true;
}

void Daemon::error( Socket socket ) {
    Logger::log( id(), info( socket ) + " got itself into error state" );
    reset( socket->address() );
}

void Daemon::renegade( Message &message ) {
    Address culprit;

    message.storeTo()
        >> culprit;

    Logger::log( id(), std::string( "peer " ) + culprit.value() + " got itself into error state" );
    reset( culprit );
}

void Daemon::table() {
#ifdef DEBUG
    std::cout << "-- table begin --" << std::endl;

    std::lock_guard< std::mutex > lock( connections().mutex() );

    for ( int i = 0; i < worldSize(); ++i ) {

        std::cout << i << ": ";
        if ( i == id() )
            std::cout << name() << " (self)";
        else {
            Socket peer = connections().find( i );
            if ( !peer )
                std::cout << "-- not assigned --";
            else
                std::cout << net().peerName( *peer );
        }

        if ( i == 0 )
            std::cout << "(master)";
        std::cout << std::endl;

    }

    std::cout << "-- table end --" << std::endl;
#endif
}

void Daemon::initData( Message &message ) {
    NOTE();
    if ( _state != State::Grouped ) {
        Logger::log( id(), "command INITDATA came at bad moment" );
        return;
    }
    message.disown();
    message.resetReading();
    auto *v = message.read();
    if ( v ) {
        _initData.reset( v->data() );
        _initDataLength = v->size();
    }
}

void Daemon::runMain( Message &message, Socket socket ) {
    Message response( MessageType::Control );
    if ( _state != State::Grouped ) {
        Logger::log( id(), "command RUN came at bad moment" );
        response.tag( Code::REFUSE );
        socket->send( response );
        return;
    }
    response.tag( Code::ACK );
    socket->send( response );

    socket->receive( response );
    if ( response.tag< Code >() != Code::START ) {
        Logger::log( id(), std::string( "invalid response from master, extected START, got " ) + codeToString( response.tag< Code >() ) );
        reset();
        return;
    }

    std::vector< char * > arguments;
    std::vector< std::unique_ptr< char > > argumentStorage;

    argumentStorage.reserve( message.count() );
    arguments.reserve( message.count() );

    for ( const auto &arg : message ) {
        argumentStorage.emplace_back( new char[ arg.size() + 1 ]() );
        arguments.emplace_back( argumentStorage.back().get() );
        std::memcpy( arguments.back(), arg.data(), arg.size() );
    }

    {
        brick::net::Redirector stdOutput( 1, [this] ( char *s, size_t length ) {
            redirectOutput( s, length, Output::Standard );
        }  );
        brick::net::Redirector stdError( 2, [this] ( char *s, size_t length ) {
            redirectOutput( s, length, Output::Error );
        }  );

        _main( arguments.size(), &arguments.front() );

    }
    response.tag( Code::DONE );
    socket->send( response );
}

void Daemon::reset() {
    Logger::log( id(), "forced to reset by circumstances" );
    Message message( MessageType::Control );
    message.tag( Code::ERROR );

    {
        std::lock_guard< std::mutex > lock( connections().mutex() );
        for ( auto i = connections().vbegin(); i != connections().end(); ++i ) {
            (*i)->send( message, true );
        }
    }
    setDefault();
}

void Daemon::reset( Address address ) {
    Logger::log( id(), std::string( "forced to reset by " ) + address.value() );
    Message message( MessageType::Control );
    message.tag( Code::RENEGADE );
    message
        << address;

    {
        std::lock_guard< std::mutex > lock( connections().mutex() );
        for ( auto i = connections().vbegin(); i != connections().end(); ++i ) {
            (*i)->send( message, true );
        }
    }
    setDefault();
}

void Daemon::setDefault() {
    connections().lockedClear();
    _rope.reset();
    if ( _state == State::Grouped || _state == State::Leaving )
        _quit = true;
    _state = State::Free;
    if ( _childPid != NoChild ) {
        int status = 0;
        Logger::log( id(), "waiting for child" );
        ::waitpid(_childPid, &status, WNOHANG);
        if ( !WIFEXITED( status ) && !WIFSIGNALED( status ) ) {
            Logger::log( id(), "kill child" );
            ::kill( _childPid, 9 ); // just kill our child
        }


        _childPid = NoChild;
    }
    id( 0 );
    worldSize( 0 );
}

void Daemon::becomeChild( Socket rope ) {
    net().unbind();
    _state = State::Grouped;
    _self = this;
    _rope = std::move( rope );
    _rope->name( name() + " (parent)" );
    Logger::becomeChild();
    Logger::log( id(), "child born" );
}

void Daemon::becomeParent( Socket rope ) {
    connections().lockedClear();
    _state = State::OverWatching;
    _rope = std::move( rope );
    _rope->name( name() + " (child)" );
    Logger::log( id(), "parent arise" );
}

void Daemon::redirectOutput( char *s, size_t length, Output type ) {
    Message message(
        type == Output::Standard ?
        MessageType::OutputStandard :
        MessageType::OutputError );

    message.add( s, length );
    Socket master = connections().lockedFind( 0 );
    if ( !master ) {
        Logger::log( id(), "cannot send output data" );
        return;
    }

    master->send( message );
}
