#include <iostream>
#include <string>
#include <cstring>

#include "client.h"
#include "message.h"
#include "logger.h"

// parallel environment
// not called from loop
void Client::quit() {

    stopListening();
    while ( listenerRunning() );

// single-thread environment
    if ( !connections().empty() ) {
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

    Socket socket = connect( slave.c_str() );

    Message message( MessageType::Control );
    message.tag( Code::SHUTDOWN );

    socket->send( message );
    Message response = socket->receive();

    if ( response.tag< Code >() == Code::ACK )
        return true;
    if ( response.tag< Code >() == Code::REFUSE )
        return false;
    throw NetworkException( "invalid response" );
}

// single-thread environment
bool Client::add( std::string slaveName ) {

    if ( _established )
        return false;

    auto i = _names.find( slaveName );
    if ( i != _names.end() )
        return false;

    int slaveId = _idCounter;

    Socket slave = connect( slaveName.c_str() );

    Message message( MessageType::Control );
    message.tag( Code::ENSLAVE );
    message
        << slaveId
        << slaveName;

    slave->send( message );
    Message response = slave->receive();

    if ( response.tag< Code >() == Code::ACK ) {
        _idCounter++;

        slave->properties( slaveId, slaveName, net().peerAddress( *slave ) );

        connections().insert( slaveId, std::move( slave ) );
        _names.emplace( std::move( slaveName ), slaveId );
        worldSize( worldSize() + 1 );
        return true;
    }
    if ( response.tag< Code >() == Code::REFUSE )
        return false;
    throw NetworkException( "invalid response" );
}

// single-thread environment
bool Client::removeAll() {
    if ( _established )
        return false;

    Message message( MessageType::Control );
    message.tag( Code::DISCONNECT );

    sendAll( message );

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
    Message message( MessageType::Control );
    message.tag( Code::TABLE );

    sendAll( message );
}

// single-thread environment
bool Client::establish( int argc, char **argv, void *initData, size_t initDataLength ) {
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

    Message message( MessageType::Control );
    message.tag( Code::PREPARE );

    for ( const auto &slave : connections().values() ) {
        slave->send( message );
        Message response = slave->receive();

        if ( response.tag< Code >() == Code::ACK )
            continue;
        if ( response.tag< Code >() == Code::REFUSE ) {
            std::cout << "! slave " << info( slave ) << " refused to prepare" << std::endl;
            break;
        }
        std::cout << "! protocol error - invalid response" << std::endl
                  << "expected ACK or REFUSE, got " << codeToString( response.tag< Code >() ) << std::endl;
        break;
    }

    message.tag( Code::LEAVE );
    for ( const auto &slave : connections().values() ) {
        slave->send( message );
        Message response = slave->receive();

        if ( response.tag< Code >() == Code::ACK )
            continue;
        if ( response.tag< Code >() == Code::REFUSE ) {
            std::cout << "! slave " << info( slave ) << " refused to leave" << std::endl;
            break;
        }
        std::cout << "! protocol error - invalid response" << std::endl
                  << "expected ACK or REFUSE, got " << codeToString( response.tag< Code >() ) << std::endl;
        break;
    }
    connections().clear();
    _names.clear();
    return true;
}

// single environment
bool Client::initWorld() {
    int world = worldSize();

    Message message( MessageType::Control );
    message.tag( Code::PEERS );
    message
        << world;

    for ( const auto &slave : connections().values() ) {
        slave->send( message );
        Message response = slave->receive();

        if ( response.tag< Code >() == Code::ACK )
            continue;
        if ( response.tag < Code >() == Code::REFUSE ) {
            Logger::log( id(), "slave '" + slave->name() + "' refused to beign grouped" );
            return false;
        }
        throw NetworkException( "invalid response" );
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

            Message message( MessageType::Control );
            message.tag( Code::CONNECT );
            message
                << id
                << slaveName
                << address;

            (*i)->send( message );
            Message response = (*i)->receive();

            if ( response.tag< Code >() == Code::SUCCESS )
                continue;
            return false;
        }
    }

    Message message( MessageType::Control );
    message.tag( Code::GROUPED );
    for ( const auto &slave : connections().values() ) {
        slave->send( message );
        Message response = slave->receive();
    }

    return true;
}

// single environment
bool Client::start( int argc, char **argv, void *initData, size_t initDataLength ) {
    if ( initData ) {
        Message data( MessageType::Control );
        data.add( initData, initDataLength );

        sendAll( data );
    }

    Message message( MessageType::Control );
    message.tag( Code::RUN );
    for ( int i = 0; i < argc; ++i )
        message.add( argv[ i ], std::strlen( argv[ i ] ) );

    for ( const auto &slave : connections().values() ) {
        slave->send( message );
        Message response = slave->receive();
        if ( response.tag< Code >() != Code::ACK )
            return false;
    }

    message.reset();
    message.tag( Code::START );
    for ( const auto &slave : connections().values() )
        slave->send( message );
    return true;
}

// single environment
void Client::reset() {
    _idCounter = 1;
    _done = 0;
    _established = false;
    stopListening();

    connections().clear();
    _names.clear();
}

// single environment
void Client::reset( Address address ) {
    Message message( MessageType::Control );
    message.tag( Code::RENEGADE );
    message
        << address;
    {
        std::lock_guard< std::mutex > lock( connections().mutex() );
        for ( const auto &slave : connections().values() ) {
            slave->send( message, true );
        }
    }
    reset();
}

// single-thread environment
bool Client::processControl( Message &message, Socket socket ) {
    switch ( message.tag< Code >() ) {
    case Code::DONE:
        return done();
    case Code::ERROR:
        error( socket );
        return false;
    case Code::RENEGADE:
        renegade( message, socket );
        return false;
    default:
        break;
    }
    return true;
}

// single-thread environment
void Client::processDisconnected( Socket socket ) {
    Logger::log( id(), "closed connection to " + info( socket ) );
    reset();
}

// single-thread environment
bool Client::done() {
    ++_done;
    if ( _done >= connections().size() ) {
        stopListening();
        return false;
    }
    return true;
}

// single-thread environment
void Client::error( Socket socket ) {
    Logger::log( id(), info( socket ) + " got itself into error state" );
    reset( socket->address() );
}

// single-thread environment
void Client::renegade( Message &message, Socket socket ) {
    Address culprit;

    message.storeTo()
        >> culprit;

    Logger::log( id(), std::string( "slave " ) + culprit.value() + " got itself into error state" );
    reset( culprit );
}
