#include <iostream>
#include <string>
#include <cstring>

#include "client.h"
#include "message.h"

// parallel environment
// not called from loop
void Client::quit() {

    if ( !connections().lockedEmpty() ) {
        if ( _established )
            dissolve();
        else
            removeAll();
    }
}

// single-thread environment
bool Client::shutdown( const std::string &slave ) {

    if ( _established )
        return false;

    auto i = _names.find( slave );
    if ( i != _names.end() )
        return false;

    Channel channel = connect( slave.c_str() );

    OutputMessage message( MessageType::Control );
    message.tag( Code::Shutdown );

    channel->send( message );

    InputMessage response;
    channel->receiveHeader( response );

    if ( response.tag< Code >() == Code::OK )
        return true;
    if ( response.tag< Code >() == Code::Refuse )
        return false;
    throw ResponseException( { Code::OK, Code::Refuse }, response.tag< Code >() );
}

void Client::forceShutdown( const std::string &slave ) {
    Channel channel = connect( slave.c_str() );

    OutputMessage message( MessageType::Control );
    message.tag( Code::ForceShutdown );

    channel->send( message );
}

// single-thread environment
bool Client::add( std::string slaveName ) {

    if ( _established )
        return false;

    auto i = _names.find( slaveName );
    if ( i != _names.end() )
        return false;

    int slaveId = _idCounter;

    Channel channel = connect( slaveName.c_str() );

    OutputMessage message( MessageType::Control );
    message.tag( Code::Enslave );
    message
        << slaveId
        << slaveName;

    channel->send( message );
    InputMessage response;
    channel->receiveHeader( response );

    if ( response.tag< Code >() == Code::OK ) {
        _idCounter++;

        std::string slaveAddress( net().peerAddress( *channel ) );
        Line slave = std::make_shared< Peer >(
            slaveId,
            slaveName,
            slaveAddress.c_str(),
            std::move( channel )
         );

        connections().insert( slaveId, std::move( slave ) );
        _names.emplace( std::move( slaveName ), slaveId );
        worldSize( worldSize() + 1 );
        return true;
    }
    if ( response.tag< Code >() == Code::Refuse )
        return false;
    throw ResponseException( { Code::OK, Code::Refuse }, response.tag< Code >() );
}

// single-thread environment
bool Client::removeAll() {
    if ( _established )
        return false;

    OutputMessage message( MessageType::Control );
    message.tag( Code::Disconnect );

    sendAll( message, ChannelType::Control );

    connections().clear();
    _names.clear();
    return true;
}

// single-thread environment
void Client::list() {
    std::cout << "== list of slaves ==" << std::endl;
    for ( const auto &slave : connections().values() ) {
        std::cout << "\t" << slave->id() << " --> "
                  << info( slave )
                  << std::endl;
    }
}

// single-thread environment
void Client::table() {
    OutputMessage message( MessageType::Control );
    message.tag( Code::Table );

    sendAll( message, ChannelType::Control );
}

// single-thread environment
void Client::run( int argc, char **argv, const void *initData, size_t initDataLength ) {
    try {
        establish( argc, argv, initData, initDataLength );

        _quit = false;
        refreshCache();

        while ( !_quit ) {
            probe( _cache, []( Channel channel ) {}, false, 2000 );
        }
        dissolve();
    } catch ( brick::net::NetException &e ) {
        std::cerr << "network exception: " << e.what() << std::endl;
    } catch ( std::exception &e ) {
        std::cerr << "exception: " << e.what() << std::endl;
    }
}

// single-thread environment
bool Client::establish( int argc, char **argv, const void *initData, size_t initDataLength ) {
    if ( _established || connections().empty() )
        return false;

    if ( !initWorld() ) {
        reset();
        return false;
    }

    if ( !connectAll() ) {
        reset();
        return false;
    }
    _established = true;
    if ( !start( argc, argv, initData, initDataLength ) ) {
        reset();
        return false;
    }

    return true;
}

// single-thread environment
bool Client::dissolve() {

    if ( !_established )
        return false;

    _established = false;

    OutputMessage message( MessageType::Control );
    message.tag( Code::PrepareToLeave );

    for ( const auto &slave : connections().values() ) {
        slave->control()->send( message );
        InputMessage response;
        slave->control()->receiveHeader( response );

        if ( response.tag< Code >() == Code::OK )
            continue;
        if ( response.tag< Code >() == Code::Refuse ) {
            std::cout << "! slave " << info( slave ) << " refused to prepare" << std::endl;
            break;
        }
        throw ResponseException( { Code::OK, Code::Refuse }, response.tag< Code >() );
    }

    message.tag( Code::Leave );
    for ( const auto &slave : connections().values() ) {
        slave->control()->send( message );
        InputMessage response;
        slave->control()->receiveHeader( response );

        if ( response.tag< Code >() == Code::OK )
            continue;
        if ( response.tag< Code >() == Code::Refuse ) {
            std::cout << "! slave " << info( slave ) << " refused to leave" << std::endl;
            break;
        }
        throw ResponseException( { Code::OK, Code::Refuse }, response.tag< Code >() );
    }
    connections().clear();
    _names.clear();
    return true;
}

// single environment
bool Client::initWorld() {
    int world = worldSize();

    OutputMessage message( MessageType::Control );
    message.tag( Code::Peers );
    message << world;

    for ( const auto &slave : connections().values() ) {
        slave->control()->send( message );
        InputMessage response;
        slave->control()->receiveHeader( response );

        if ( response.tag< Code >() == Code::OK )
            continue;
        if ( response.tag < Code >() == Code::Refuse ) {
            std::cerr << "slave '" << slave->name() << "' refused to beign grouped" << std::endl;
            return false;
        }
        throw ResponseException( { Code::OK, Code::Refuse }, response.tag< Code >() );
    }
    return true;
}

// single environment
bool Client::connectAll() {
    for ( auto i = connections().vbegin(); i != connections().vend(); ++i ) {

        auto p = i;
        ++p;
        for ( ; p != connections().end(); ++p ) {
            int id = (*p)->id();
            std::string slaveName = (*p)->name();
            Address address = (*p)->address();

            OutputMessage message( MessageType::Control );
            message.tag( Code::ConnectTo );
            message << id << slaveName << address << _channels;

            (*i)->control()->send( message );
            InputMessage response;
            (*i)->control()->receiveHeader( response );

            switch( response.tag< Code >() ) {
            case Code::Success:
                break;
            case Code::Refuse:
                return false;
            default:
                throw ResponseException( { Code::OK, Code::Refuse }, response.tag< Code >() );
            }
        }
    }

    OutputMessage message( MessageType::Control );
    message.tag( Code::Grouped );
    for ( const auto &slave : connections().values() ) {
        slave->control()->send( message );
        InputMessage response;
        slave->control()->receiveHeader( response );

        switch( response.tag< Code >() ) {
        case Code::OK:
            break;
        case Code::Refuse:
            return false;
        default:
            throw ResponseException( { Code::OK, Code::Refuse }, response.tag< Code >() );
        }
    }

    return true;
}

// single environment
bool Client::start( int argc, char **argv, const void *initData, size_t initDataLength ) {
    if ( initData ) {
        OutputMessage data( MessageType::Control );
        data.tag( Code::InitialData );
        data.add( initData, initDataLength );

        sendAll( data, ChannelType::Control );
    }

    OutputMessage message( MessageType::Control );
    message.tag( Code::Run );
    for ( int i = 0; i < argc; ++i )
        message.add( argv[ i ], std::strlen( argv[ i ] ) );

    for ( const auto &slave : connections().values() ) {
        slave->control()->send( message );
        InputMessage response;
        slave->control()->receiveHeader( response );

        switch( response.tag< Code >() ) {
        case Code::OK:
            break;
        case Code::Refuse:
            return false;
        default:
            throw ResponseException( { Code::OK, Code::Refuse }, response.tag< Code >() );
        }
    }

    message.clear();
    message.tag( Code::Start );
    for ( const auto &slave : connections().values() )
        slave->control()->send( message );
    return true;
}

// single environment
void Client::reset() {
    _idCounter = 1;
    _done = 0;
    _quit = true;
    _established = false;

    connections().clear();
    _names.clear();
}

// single environment
void Client::reset( Address address ) {
    OutputMessage message( MessageType::Control );
    message.tag( Code::Renegade );
    message << address;
    {
        std::lock_guard< std::mutex > lock( connections().mutex() );
        for ( const auto &slave : connections().values() ) {
            slave->control()->send( message, true );
        }
    }
    reset();
}

// single-thread environment
bool Client::processControl( Channel channel ) {
    InputMessage message;
    channel->peek( message );

    switch ( message.tag< Code >() ) {
    case Code::Done:
        channel->receiveHeader( message );
        return done();
    case Code::Error:
        channel->receiveHeader( message );
        error( channel );
        return false;
    case Code::Renegade:
        renegade( message, channel );
        return false;
    default:
        break;
    }
    return true;
}

// single-thread environment
void Client::processDisconnected( Channel channel ) {
    std::cerr << "closed connection to " << info( channel ) << std::endl;
    reset();
}

// single-thread environment
void Client::processOutput( Channel channel ) {
    InputMessage message;
    channel->receive( message );

    std::ostream &out = message.tag< Output >() == Output::Standard ?
        std::cout :
        std::cerr;

    message.process( [&] ( char *data, size_t size ) {
        out.write( data, size );
    } );

    message.cleanup< char >( []( char *d ) {
        delete[] d;
    } );
}

// single-thread environment
bool Client::done() {
    ++_done;
    if ( _done >= connections().size() ) {
        _quit = true;
        return false;
    }
    return true;
}

// single-thread environment
void Client::error( Channel channel ) {
    std::cerr << info( channel ) << " got itself into error state" << std::endl;
    reset();
}

// single-thread environment
void Client::renegade( InputMessage &message, Channel channel ) {
    Address culprit;

    message >> culprit;
    channel->receive( message );

    std::cerr << "slave " << culprit.value() << " got itself into error state" << std::endl;
    reset( culprit );
}

void Client::refreshCache() {
    std::vector< Channel > cache;
    cache.reserve( connections().size() );
    for ( Line &line : connections().values() ) {
        if ( line->controlChannel() )
            cache.push_back( line->control() );
    }
    _cache.swap( cache );
}
