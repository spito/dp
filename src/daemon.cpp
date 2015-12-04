#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <chrono>

#include "daemon.h"

std::unique_ptr< Daemon > Daemon::_self;

Daemon::Daemon( const char *port, int (*main)( int, char ** ), std::string logFile ) :
    Communicator( port, true ),
    _state( State::Free ),
    _childPid( NoChild ),
    _quit( false ),
    _main( main )
{
    Logger::file( std::move( logFile ) );
}

Daemon &Daemon::instance() {
    if ( !_self )
        throw std::runtime_error( "daemon is not instanciated" );
    return *_self;
}

Daemon &Daemon::instance( const char *port, int (*main)( int, char ** ), std::string logFile ) {
    if ( _self )
        throw std::runtime_error( "cannot instanciate daemon multiple times" );
    _self.reset( new Daemon( port, main, std::move( logFile ) ) );
    return *_self;
}

bool Daemon::hasInstance() {
    return bool( _self );
}

void Daemon::exit( int code ) {
    NOTE();
    Line master = connections().lockedFind( 0 );
    if ( !master ) {
        Logger::log( "inaccessible master" );
        ::exit( -code );
    }

    OutputMessage notification( MessageType::Control );
    notification.tag( Code::Done );
    master->master()->send( notification );

    while ( !_quit ) {
        Communicator::probe(
            _cache.at( ChannelID( ChannelType::All ).asIndex() ),
            [this]( Channel channel ) {
                this->consumeMessage( channel );
            },
            -1,
            false
        );
    }
    ::exit( code );
}

void Daemon::run() {
    NOTE();

    if ( !daemonize() )
         return;

    while ( !_quit ) {
        std::vector< Channel > channels;
        if ( _childPid != NoChild )
            channels.push_back( _rope );

        {
            std::lock_guard< std::mutex > _( connections().mutex() );
            channels.reserve( channels.size() + connections().size() );

            for ( Line &line : connections().values() ) {
                if ( line->masterChannel() )
                    channels.push_back( line->master() );
            }
        }

        try {
            Communicator::probe(
                channels,
                [this]( Channel channel ) {
                    this->consumeMessage( channel );
                },
                -1,
                true
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

bool Daemon::daemonize() {
    int pid, sid;

    pid = ::fork();
    if ( pid < 0 )
        throw brick::net::SystemException( "fork" );

    if ( pid > 0 )
        return false;
    ::umask( 0 );
    sid = ::setsid();
    if ( sid < 0 ) {
        Logger::log( "cannot set sid" );
        return false;
    }
    ::close( STDIN_FILENO );
    ::close( STDOUT_FILENO );
    ::close( STDERR_FILENO );

    int fd;
    fd = ::open( "/dev/null", O_RDONLY );
    if ( fd != STDIN_FILENO ) {
        ::dup2( fd, STDIN_FILENO );
        ::close( fd );
    }

    fd = ::open( "/dev/null", O_WRONLY );
    if ( fd != STDOUT_FILENO ) {
        ::dup2( fd, STDOUT_FILENO );
        ::close( fd );
    }

    fd = ::open( "/dev/null", O_WRONLY );
    if ( fd != STDERR_FILENO ) {
        ::dup2( fd, STDERR_FILENO );
        ::close( fd );
    }

    return true;
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
    case Code::Status:
        cleanup();
        status( std::move( channel ) );
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
            Code::InitialData,
            Code::Run,
        }, code );
        break;
    }
    return true;
}

void Daemon::processDisconnected( Channel dead ) {
    switch ( _state ) {
    case State::Leaving:
        break;
    case State::Supervising:
        NOTE();
        if ( _rope == dead )
            Logger::log( "working child died" );
        else
            Logger::log( "internal error happened - other connection than rope to the child has died" );
        setDefault();
        break;
    default:
        NOTE();
        Logger::log( "closed connection to " + info( dead ) );
        ::exit( 0 );
    }
}

void Daemon::processIncoming( Channel incoming ) {
    processControl( std::move( incoming ) );
}

void Daemon::enslave( InputMessage &message, Channel channel ) {
    NOTE();

    OutputMessage response( MessageType::Control );
    if ( _state != State::Free ) {
        response.tag( Code::Refuse );
        std::string description = status();
        response << description;

        channel->send( response );
        channel->close();
        return;
    }

    int i;
    std::string selfName;
    int openChannels;
    message >> i >> selfName >> openChannels;
    channel->receive( message );


    response.tag( Code::OK );
    channel->send( response );

    id( i );
    name( selfName );
    channels( openChannels );

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

    message >> peerId >> peerName >> peerAddress;
    channel->receive( message );

    if ( _state != State::FormingGroup ) {
        channel->send( response );
        return;
    }

    Channel masterLine = connectLine( peerAddress, LineType::Master );
    if ( masterLine ) {
        Line peer = std::make_shared< Peer >(
            peerId,
            std::move( peerName ),
            peerAddress.value(),
            std::move( masterLine ),
            channels()
        );

        bool ok = true;

        for ( int i = 0; i < channels(); ++i ) {
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
    case LineType::Master:
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
    if ( _state != State::Supervising || channel != _rope ) {
        Logger::log( "command CutRope came from bad source or in a bad moment" );
        return;
    }
    setDefault( true );
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

void Daemon::status( Channel channel ) {
    NOTE();

    OutputMessage response( MessageType::Control );
    response.tag( Code::Status );
    std::string description = status();

    response << description;
    channel->send( response );
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
    std::vector< std::unique_ptr< char[] > > argumentStorage;

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
            char *arg = new char[ length + 1 ]();
            argumentStorage.emplace_back( arg );
            arguments.emplace_back( arg );
            return arg;
        } );
        message.clear();

        channel->receiveHeader( message );

        if ( message.tag< Code >() != Code::Start ) {
            throw ResponseException( { Code::Start }, message.tag< Code >() );
        }
    }
    {
        brick::net::Redirector stdOutput( STDOUT_FILENO, [this] ( char *s, size_t length ) {
            this->redirectOutput( s, length, Output::Standard );
        } );
        brick::net::Redirector stdError( STDERR_FILENO, [this] ( char *s, size_t length ) {
            this->redirectOutput( s, length, Output::Error );
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
            peer->master()->send( message, true );
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
            peer->master()->send( message, true );
        }
    }
    setDefault();
}

void Daemon::setDefault( bool wait ) {
    connections().lockedClear();
    _rope.reset();
    if ( _state == State::Grouped || _state == State::Leaving )
        _quit = true;
    _state = State::Free;
    if ( _childPid != NoChild ) {
        Logger::log( "waiting for child" );

        int status = 0;
        int options = 0;
        if ( !wait )
            options = WNOHANG;

        // hope for C++14
        // std::this_thread::sleep_for( 100ms );
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
        int r = ::waitpid( _childPid, &status, options );

        if ( r == _childPid && ( WIFEXITED( status ) || WIFSIGNALED( status ) ) ) {
            Logger::log( "child was buried" );
        }
        else {
            Logger::log( "kill child" );
            ::kill( _childPid, 9 ); // just kill our child
            ::waitpid( _childPid, &status, 0 );
        }
        _childPid = NoChild;
    }
    id( 0 );
    worldSize( 0 );
}

void Daemon::becomeChild( Channel rope ) {
    net().unbind();
    _state = State::Grouped;
    _rope = std::move( rope );
    _rope->id( id() );
    Logger::becomeChild();
    Logger::log( "child born" );
    buildCache();
}

void Daemon::becomeParent( Channel rope ) {
    connections().lockedClear();
    _state = State::Supervising;
    _rope = std::move( rope );
    _rope->id( id() );
    Logger::log( "parent arise" );
}

void Daemon::consumeMessage( Channel channel ) {
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
    master->master()->send( message );
}

void Daemon::buildCache()
{
    _cache.resize( channels() + 2 );
    {
        std::vector< Channel > cache;
        for ( Line &line : connections().values() ) {
            cache.push_back( line->master() );
            for ( auto &ch : line->data() ) {
                if ( ch )
                    cache.push_back( ch );
            }
        }
        _cache[ ChannelID( ChannelType::All ).asIndex() ] = std::move( cache );
    }
    {
        std::vector< Channel > cache;
        for ( Line &line : connections().values() ) {
            if ( line->masterChannel() )
                cache.push_back( line->master() );
        }
        _cache[ ChannelID( ChannelType::Master ).asIndex() ] = std::move( cache );
    }
    for ( int chID = 0; chID < channels(); ++chID ) {
        std::vector< Channel > cache;
        std::lock_guard< std::mutex > _( connections().mutex() );
        cache.reserve( connections().size() );

        for ( Line &line : connections().values() ) {
            if ( line->dataChannel( chID ) )
                cache.push_back( line->data( chID ) );
        }
        _cache[ ChannelID( chID ).asIndex() ] = std::move( cache );
    }
}

void Daemon::table() {
    std::ostringstream out;
    out << "==[" << name() << " (" << id() << ")]==" << std::endl;
    std::lock_guard< std::mutex > _{ connections().mutex() };
    for ( Line &line : connections().values() ) {
        out << "id: " << line->id() << " name: " << line->name() << std::endl;
        out << "\tmaster: " << line->master()->fd() << std::endl;
        int i = 0;
        for ( const Channel &channel : line->data() ) {
            out << "\t" << i << ": " << channel->fd() << std::endl;
            ++i;
        }

    }
    std::cout << out.str() << std::endl;
}

const char *Daemon::status() const {
    switch ( _state ) {
    case State::Free:
        return "free";
    case State::Enslaved:
        return "enslaved";
    case State::FormingGroup:
        return "forming group";
    case State::Grouped:
        return "grouped";
    case State::Supervising:
        return "supervising";
    case State::Running:
        return "running";
    case State::Leaving:
        return "leaving";
    default:
        return "";
    }
 }
