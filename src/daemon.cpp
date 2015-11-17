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

void Daemon::exit( int code ) {

    Line master = connections().lockedFind( 0 );
    if ( !master ) {
        Logger::log( "inaccessible master" );
        ::exit( -code );
    }

    OutputMessage notification( MessageType::Control );
    notification.tag( Code::Done );
    master->control()->send( notification );

    std::vector< Channel > channel{ master->control() };

    while ( !_quit ) {
        Communicator::probe(
            channel,
            [this]( Channel channel ) {
                this->consumeMessage( channel );
            },
            false,
            -1
        );
    }
    ::exit( code );
}

void Daemon::run() {
    NOTE();

    while ( !_quit ) {
        std::vector< Channel > channels;
        if ( _childPid != NoChild )
            channels.push_back( _rope );

        {
            std::lock_guard< std::mutex > _( connections().mutex() );
            channels.reserve( channels.size() + connections().size() );

            for ( Line &line : connections().values() ) {
                if ( line->controlChannel() )
                    channels.push_back( line->control() );
            }
        }

        try {
            Communicator::probe(
                channels,
                [this]( Channel channel ) {
                    this->consumeMessage( channel );
                },
                true,
                -1
             );
        } catch ( brick::net::NetException &e ) {
            Logger::log( std::string( "got a network exception: " ) + e.what() );
            reset();
        } catch ( std::exception &e ) {
            Logger::log( std::string( "got an exception: " ) + e.what() );
            reset();
        } catch ( ... ) {
            Logger::log( "got an unhandled exception" );
            reset();
        }
    }
}


bool Daemon::processControl( Channel channel ) {
    InputMessage message;
    channel->peek( message );
    auto cleanup = [&] {
        channel->receiveHeader( message );
    };

    Code code = message.tag< Code >();

    switch ( code ) {
    case Code::Enslave:
        enslave( message, std::move( channel ) );
        break;
    case Code::Peers:
        startGrouping( message, std::move( channel ) );
        break;
    case Code::ConnectTo:
        connecting( message, std::move( channel ) );
        break;
    case Code::DataLine:
        addDataLine( message, std::move( channel ) );
        break;
    case Code::Join:
        join( message, std::move( channel ) );
        break;
    case Code::Grouped:
        cleanup();
        grouped( std::move( channel ) );
        break;
    case Code::PrepareToLeave:
        cleanup();
        prepare( std::move( channel ) );
        break;
    case Code::Leave:
        cleanup();
        leave( std::move( channel ) );
        break;
    case Code::Disconnect:
        cleanup();
        release( std::move( channel ) );
        break;
    case Code::CutRope:
        cleanup();
        cutRope( std::move( channel ) );
        break;
    case Code::Shutdown:
        cleanup();
        shutdown( std::move( channel ) );
        break;
    case Code::ForceShutdown:
        cleanup();
        forceShutdown();
        break;
    case Code::Error:
        cleanup();
        error( std::move( channel ) );
        break;
    case Code::Renegade:
        renegade( message, std::move( channel ) );
        break;
    case Code::Table:
        cleanup();
        table();
        break;
    case Code::InitialData:
        initData( message, std::move( channel ) );
        break;
    case Code::Run:
        runMain( message, std::move( channel ) );
        break;
    default:
        cleanup();
        throw ResponseException( {
            Code::Enslave,
            Code::Peers,
            Code::ConnectTo,
            Code::DataLine,
            Code::Join,
            Code::Grouped,
            Code::PrepareToLeave,
            Code::Leave,
            Code::Disconnect,
            Code::CutRope,
            Code::Shutdown,
            Code::Error,
            Code::Renegade,
            Code::Table,
            Code::InitialData,
            Code::Run
        }, code );
        break;
    }
    return true;
}

void Daemon::processDisconnected( Channel dead ) {
    NOTE();

    switch ( _state ) {
    case State::Leaving:
        break;
    case State::OverWatching:
        if ( _rope == dead )
            Logger::log( "working child died" );
        else
            Logger::log( "internal error happened - other connection than rope to the child has died" );
        setDefault();
        break;
    default:
        Logger::log( "closed connection to " + info( dead ) );
        throw NetworkException{ "connection closed" };
    }
}

void Daemon::processIncomming( Channel incomming ) {
    processControl( std::move( incomming ) );
}

void Daemon::enslave( InputMessage &message, Channel channel ) {
    NOTE();

    OutputMessage response( MessageType::Control );
    if ( _state != State::Free ) {
        response.tag( Code::Refuse );
        channel->send( response );
        channel->close();
        return;
    }

    int i;
    std::string selfName;
    message >> i >> selfName;
    channel->receive( message );


    response.tag( Code::OK );
    channel->send( response );

    id( i );
    name( selfName );

    std::string address( net().peerAddress( *channel ) );
    Line master = std::make_shared< Peer >(
        0,
        "master",
        address.c_str(),
        std::move( channel )
    );

    connections().lockedInsert( 0, std::move( master ) ); // zero for master
    _state = State::Enslaved;
}

void Daemon::startGrouping( InputMessage &message, Channel channel ) {
    NOTE();

    OutputMessage response( MessageType::Control );
    int parameter;

    message >> parameter;
    channel->receive( message );

    if ( _state != State::Enslaved ) {
        response.tag( Code::Refuse );
        channel->send( response );
        return;
    }
    response.tag( Code::OK );
    channel->send( response );

    _state = State::FormingGroup;
    worldSize( parameter );
}

void Daemon::connecting( InputMessage &message, Channel channel ) {
    NOTE();

    OutputMessage response( MessageType::Control );
    response.tag( Code::Refuse );

    int peerId;
    std::string peerName;
    Address peerAddress;
    int channels;

    message >> peerId >> peerName >> peerAddress >> channels;
    channel->receive( message );

    if ( _state != State::FormingGroup ) {
        channel->send( response );
        return;
    }

    Channel controlLine = connectLine( peerAddress, LineType::Control );
    if ( controlLine ) {
        Line peer = std::make_shared< Peer >(
            peerId,
            std::move( peerName ),
            peerAddress.value(),
            std::move( controlLine ),
            channels
        );

        bool ok = true;

        for ( int i = 0; i < channels; ++i ) {
            Channel dataLine = connectLine( peerAddress, LineType::Data, i );
            if ( !dataLine ) {
                ok = false;
                break;
            }
            peer->openDataChannel( std::move( dataLine ), i );
        }

        if ( ok ) {
            connections().lockedInsert( peerId, std::move( peer ) );
            response.tag( Code::Success );
        }
    }

    channel->send( response );
}

Channel Daemon::connectLine( Address &address, LineType type, int channelId ) {
    OutputMessage request( MessageType::Control );
    std::string name = this->name();
    int id = this->id();

    switch( type ){
    case LineType::Control:
        request.tag( Code::Join );
        request << id << name;
        break;
    case LineType::Data:
        request.tag( Code::DataLine );
        request << id << channelId;
        break;
    }

    Channel channel = connect( address );
    channel->send( request );

    InputMessage response;
    channel->receive( response );
    if ( response.tag< Code >() == Code::OK )
        return channel;
    if ( response.tag< Code >() == Code::Refuse )
        return Channel();
    throw ResponseException( { Code::OK, Code::Refuse }, response.tag< Code >() );
}

void Daemon::addDataLine( InputMessage &message, Channel channel ) {
    NOTE();

    OutputMessage response( MessageType::Control );
    response.tag( Code::Refuse );

    int peerId;
    int channelId;
    message >> peerId >> channelId;
    channel->receive( message );

    if ( _state == State::FormingGroup ) {
        Line peer = connections().lockedFind( peerId );
        if ( peer ) {
            peer->openDataChannel( channel, channelId );
            response.tag( Code::OK );
        }
    }
    channel->send( response );
}

void Daemon::join( InputMessage &message, Channel channel ) {
    NOTE();

    OutputMessage response( MessageType::Control );
    int peerId;
    std::string peerName;

    message >> peerId >> peerName;
    channel->receive( message );

    if ( _state != State::FormingGroup ) {
        response.tag( Code::Refuse );
        channel->send( response );
        return;
    }

    response.tag( Code::OK );
    channel->send( response );

    std::string address( net().peerAddress( *channel ) );
    Line peer = std::make_shared< Peer >(
        peerId,
        std::move( peerName ),
        address.c_str(),
        std::move( channel )
    );

    connections().lockedInsert( peerId, std::move( peer ) );
}

void Daemon::grouped( Channel channel ) {
    NOTE();

    OutputMessage response( MessageType::Control );
    if ( _state != State::FormingGroup ) {
        response.tag( Code::Refuse );
        channel->send( response );
        return;
    }

    Channel top, bottom;
    std::tie( top, bottom ) = Communicator::socketPair();
    top->shutdown( brick::net::Shutdown::Write );
    bottom->shutdown( brick::net::Shutdown::Read );

    response.tag( Code::OK );
    channel->send( response );

    int pid = ::fork();
    if ( pid == -1 )
        throw brick::net::SystemException( "fork" );
    if ( pid == 0 )
        becomeChild( std::move( bottom ) );
    else {
        _childPid = pid;
        becomeParent( std::move( top ) );
    }
}

void Daemon::prepare( Channel channel ) {
    NOTE();
    OutputMessage response( MessageType::Control );
    if ( _state != State::Grouped && _state != State::FormingGroup ) {
        response.tag( Code::Refuse );
        channel->send( response );
        return;
    }
    response.tag( Code::OK );
    channel->send( response );
    _state = State::Leaving;
}

void Daemon::leave( Channel channel ) {
    NOTE();

    OutputMessage response( MessageType::Control );
    if ( _state != State::Leaving ) {
        response.tag( Code::Refuse );
        channel->send( response );
        return;
    }

    response.tag( Code::OK );
    channel->send( response );

    response.tag( Code::CutRope );
    _rope->send( response );

    _state = State::Enslaved;
    worldSize( 0 );
    _quit = true;
}

void Daemon::release( Channel channel ) {
    NOTE();
    OutputMessage response( MessageType::Control );
    if ( _state != State::Enslaved ) {
        response.tag( Code::Refuse );
        channel->send( response );
        return;
    }
    response.tag( Code::OK );
    channel->send( response );

    setDefault();
    return;
}

void Daemon::cutRope( Channel channel ) {
    NOTE();
    if ( _state != State::OverWatching || channel != _rope ) {
        Logger::log( "command CutRope came from bad source or in a bad moment" );
        return;
    }
    setDefault();
    return;
}

void Daemon::shutdown( Channel channel ) {
    NOTE();
    OutputMessage response( MessageType::Control );
    if ( _state != State::Free ) {
        response.tag( Code::Refuse );
        channel->send( response );
        return;
    }
    response.tag( Code::OK );
    channel->send( response );
    reset();
    ::exit( 0 );
}

void Daemon::forceShutdown() {
    NOTE();
    reset();
    ::exit( 0 );
}

void Daemon::error( Channel channel ) {
    Logger::log( info( channel ) + " got itself into error state" );
    Line peer = connections().lockedFind( channel->id() );
    if ( peer )
        reset( peer->address() );
    else
        reset();
}

void Daemon::renegade( InputMessage &message, Channel channel ) {
    Address culprit;

    message >> culprit;
    channel->receive( message );

    Logger::log( std::string( "peer " ) + culprit.value() + " got itself into error state" );
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

void Daemon::initData( InputMessage &message, Channel channel ) {
    NOTE();
    if ( _state != State::Grouped ) {
        Logger::log( "command InitialData came at bad moment" );
        return;
    }

    channel->receive( message, [this]( size_t length ) {
        _initData.reset( new char[ length ] );
        _initDataLength = length;
        return _initData.get();
    } );
}

void Daemon::runMain( InputMessage &message, Channel channel ) {
    NOTE();

    std::vector< char * > arguments;
    std::vector< std::unique_ptr< char > > argumentStorage;

    {
        OutputMessage response( MessageType::Control );
        if ( _state != State::Grouped ) {
            Logger::log( "command Run came at bad moment" );
            response.tag( Code::Refuse );
            channel->send( response );
            return;
        }
        response.tag( Code::OK );
        channel->send( response );


        channel->receive( message, [&,this] ( size_t length ) {
            argumentStorage.emplace_back( new char[ length + 1 ]() );
            arguments.emplace_back( argumentStorage.back().get() );
            return arguments.back();
        } );
        message.clear();

        channel->receiveHeader( message );

        if ( message.tag< Code >() != Code::Start ) {
            throw ResponseException( { Code::Start }, message.tag< Code >() );
        }
    }
    {
        brick::net::Redirector stdOutput( 1, [this] ( char *s, size_t length ) {
            redirectOutput( s, length, Output::Standard );
        } );
        brick::net::Redirector stdError( 2, [this] ( char *s, size_t length ) {
            redirectOutput( s, length, Output::Error );
        } );

        _main( arguments.size(), &arguments.front() );

    }
    exit();
}

void Daemon::reset() {
    Logger::log( "forced to reset by circumstances" );
    OutputMessage message( MessageType::Control );
    message.tag( Code::Error );

    {
        std::lock_guard< std::mutex > lock( connections().mutex() );
        for ( Line &peer : connections().values() ) {
            peer->control()->send( message, true );
        }
    }
    setDefault();
}

void Daemon::reset( Address address ) {
    Logger::log( std::string( "forced to reset by " ) + address.value() );
    OutputMessage message( MessageType::Control );
    message.tag( Code::Renegade );
    message
        << address;

    {
        std::lock_guard< std::mutex > lock( connections().mutex() );
        for ( Line &peer : connections().values() ) {
            peer->control()->send( message, true );
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
        Logger::log( "waiting for child" );
        ::waitpid(_childPid, &status, WNOHANG);
        if ( !WIFEXITED( status ) && !WIFSIGNALED( status ) ) {
            Logger::log( "kill child" );
            ::kill( _childPid, 15 ); // just kill our child
        }
        _childPid = NoChild;
    }
    id( 0 );
    worldSize( 0 );
}

void Daemon::becomeChild( Channel rope ) {
    net().unbind();
    _state = State::Grouped;
    _self = this;
    _rope = std::move( rope );
    _rope->id( id() );
    Logger::becomeChild();
    Logger::log( "child born" );
}

void Daemon::becomeParent( Channel rope ) {
    connections().lockedClear();
    _state = State::OverWatching;
    _rope = std::move( rope );
    _rope->id( id() );
    Logger::log( "parent arise" );
}

void Daemon::consumeMessage( Channel channel ) {
    NOTE();
    InputMessage message;
    channel->receiveHeader( message );
}

void Daemon::redirectOutput( const char *s, size_t length, Output type ) {
    OutputMessage message( MessageType::Output );

    message.tag( type );
    message.add( s, length );
    Line master = connections().lockedFind( 0 );
    if ( !master ) {
        Logger::log( "cannot send output data" );
        return;
    }
    master->control()->send( message );
}

auto Daemon::refreshCache( ChannelID chID )
    -> std::unordered_map< int, std::vector< Channel > >::iterator
{
    std::vector< Channel > cache;
    {
        std::lock_guard< std::mutex > _( connections().mutex() );
        cache.reserve( connections().size() );

        switch ( chID.asType() ) {
        case ChannelType::Control:
            for ( Line &line : connections().values() ) {
                if ( line->controlChannel() )
                    cache.push_back( line->control() );
            }
            break;
        case ChannelType::DataAll:
            for ( Line &line : connections().values() ) {
                for ( auto &ch : line->data() ) {
                    if ( ch )
                        cache.push_back( ch );
                }
            }
            break;
        default:
            for ( Line &line : connections().values() ) {
                if ( line->dataChannel( chID ) )
                    cache.push_back( line->data( chID ) );
            }
            break;
        }
    }
    return _cache.emplace( chID, std::move( cache ) ).first;
}
