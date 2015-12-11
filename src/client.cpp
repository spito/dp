#include <iostream>
#include <string>
#include <sstream>
#include <cstring>
#include <iomanip>

#include "client.h"
#include "message.h"

void Client::quit() {

    if ( !connections().lockedEmpty() ) {
        if ( _established )
            dissolve();
        else
            removeAll();
    }
}

bool Client::shutdown( const std::string &machine ) {

    if ( _established )
        return false;

    auto i = _names.find( machine );
    if ( i != _names.end() )
        return false;

    Channel channel = connect( machine.c_str() );

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

void Client::forceShutdown( const std::string &machine ) {
    Channel channel = connect( machine.c_str() );

    OutputMessage message( MessageType::Control );
    message.tag( Code::ForceShutdown );

    channel->send( message );
}

std::vector< bool > Client::start( const std::vector< std::string > &machines ) {

    std::vector< std::future< bool > > handles;
    std::vector< bool > result( machines.size(), false );

    char path[ 256 ] = {}; // FIXME: add some system constant
    if ( ::readlink( "/proc/self/exe", path, 255 ) == -1 )
        return result;

    handles.reserve( machines.size() );

    brick::net::Redirect out{ STDOUT_FILENO };
    brick::net::Redirect err{ STDERR_FILENO };

    for ( const auto &m : machines ) {
        handles.emplace_back( std::async( std::launch::async, [&] {
            std::ostringstream os;
            os  << "ssh -T " << m << "<<'ENDSSH'\n"
                << path << " d -l " << m << "\n"
                << "ENDSSH";
            std::string command{ os.str() };
            return ::system( command.c_str() ) == 0;
        } ) );
    }

    std::transform( handles.begin(), handles.end(), result.begin(), []( std::future< bool > &h ) {
        return h.get();
    } );
    return result;
}

bool Client::add( std::string slaveName, std::string *description ) {

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
        << slaveName
        << channels();

    channel->send( message );
    InputMessage response;

    if ( description ) {
        response >> *description;
        channel->receive( response );
    }
    else
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

bool Client::removeAll() {
    if ( _established )
        return false;

    OutputMessage message( MessageType::Control );
    message.tag( Code::Disconnect );

    if ( !sendAll( message, ChannelType::Master ) )
        return false;

    connections().clear();
    _names.clear();
    return true;
}

std::string Client::status( const std::string &machine ) {
    Channel channel = connect( machine.c_str(), true );
    if ( !channel || !*channel )
        return "not available";

    OutputMessage message( MessageType::Control );
    message.tag( Code::Status );

    channel->send( message );

    InputMessage response;
    std::string description;
    response >> description;

    channel->receive( response );
    return description;
}

void Client::run( int argc, char **argv, const void *initData, size_t initDataLength ) {
    try {
        if ( !establish( argc, argv, initData, initDataLength ) ) {
            std::cerr << "could not establish the network" << std::endl;
            return;
        }

        _quit = false;
        refreshCache();

        while ( !_quit ) {
            probe( _cache, &Client::discardMessage, 2000, false );
        }
        dissolve();
    } catch ( brick::net::NetException &e ) {
        std::cerr << "network exception: " << e.what() << std::endl;
    } catch ( std::exception &e ) {
        std::cerr << "exception: " << e.what() << std::endl;
    }
}

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

bool Client::dissolve() {

    if ( !_established )
        return false;

    _established = false;

    OutputMessage message( MessageType::Control );
    message.tag( Code::PrepareToLeave );

    for ( const auto &slave : connections().values() ) {
        slave->master()->send( message );
        InputMessage response;
        slave->master()->receiveHeader( response );

        if ( response.tag< Code >() == Code::OK )
            continue;
        if ( response.tag< Code >() == Code::Refuse ) {
            std::cout << "! slave " << info( slave ) << " refused to prepare to leave" << std::endl;
            break;
        }
        throw ResponseException( { Code::OK, Code::Refuse }, response.tag< Code >() );
    }

    message.tag( Code::Leave );
    for ( const auto &slave : connections().values() ) {
        slave->master()->send( message );
        InputMessage response;
        slave->master()->receiveHeader( response );

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

bool Client::initWorld() {
    int world = worldSize();

    OutputMessage message( MessageType::Control );
    message.tag( Code::Peers );
    message << world;

    for ( const auto &slave : connections().values() ) {
        slave->master()->send( message );
        InputMessage response;
        slave->master()->receiveHeader( response );

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
            message << id << slaveName << address;

            (*i)->master()->send( message );
            InputMessage response;
            (*i)->master()->receiveHeader( response );

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
        slave->master()->send( message );
        InputMessage response;
        slave->master()->receiveHeader( response );

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

bool Client::start( int argc, char **argv, const void *initData, size_t initDataLength ) {
    if ( initData ) {
        OutputMessage data( MessageType::Control );
        data.tag( Code::InitialData );
        data.add( initData, initDataLength );

        if ( !sendAll( data, ChannelType::Master ) )
            throw SendAllException();
    }

    OutputMessage message( MessageType::Control );
    message.tag( Code::Run );
    for ( int i = 0; i < argc; ++i )
        message.add( argv[ i ], std::strlen( argv[ i ] ) );

    for ( const auto &slave : connections().values() ) {
        slave->master()->send( message );
        InputMessage response;
        slave->master()->receiveHeader( response );

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
        slave->master()->send( message );
    return true;
}

void Client::reset() {
    _idCounter = 1;
    _done = 0;
    _quit = true;
    _established = false;

    connections().clear();
    _names.clear();
}

void Client::reset( Address address ) {
    OutputMessage message( MessageType::Control );
    message.tag( Code::Renegade );
    message << address;
    {
        std::lock_guard< std::mutex > lock( connections().mutex() );
        for ( const auto &slave : connections().values() ) {
            slave->master()->send( message, true );
        }
    }
    reset();
}

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

void Client::processDisconnected( Channel channel ) {
    std::cerr << "closed connection to " << info( channel ) << std::endl;
    reset();
}

void Client::processOutput( Channel channel ) {
    InputMessage message;
    channel->receive( message );

    std::ostream &out = message.tag< Output >() == Output::Standard ?
        std::cout :
        std::cerr;

    message.process( [&] ( char *data, size_t size ) {
        out.write( data, size );
        out.flush();
    } );

    message.cleanup< char >( []( char *d ) {
        delete[] d;
    } );
}

bool Client::done() {
    ++_done;
    if ( _done >= connections().size() ) {
        _quit = true;
        return false;
    }
    return true;
}

void Client::error( Channel channel ) {
    std::cerr << info( channel ) << " got itself into error state" << std::endl;
    reset();
}

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
        if ( line->masterChannel() )
            cache.push_back( line->master() );
    }
    _cache.swap( cache );
}

void Client::discardMessage( Channel channel ) {
    InputMessage message;
    channel->receiveHeader( message );
    // channel->receive( message );
    // std::cout << "discarded message ("
    //     << "from: " << message.from() << ", "
    //     << "to: " << message.to() << ", "
    //     << "count: " << message.count() << ", "
    //     << "tag:" << message.tag() << ")" << std::endl;
    // message.process( []( char *data, size_t s ) {
    //     std::cout << "@ " << std::hex << std::setfill( '0' );
    //     for ( int i = 0; i < s; ++i )
    //         std::cout << std::setw( 2 ) << int( data[ i ] );
    //     std::cout << std::dec << std::setfill( ' ' ) << std::endl;
    // } );
    // message.cleanup< char >( []( char *d ){ delete[] d; } );
}