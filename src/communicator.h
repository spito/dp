#include <type_traits>
#include <chrono>
#include <algorithm>

#include <brick-net.h>
#include <brick-unittest.h>
//#include <brick-db.h>

#include "connections.h"
#include "message.h"
#include "logger.h"

#ifndef COMMUNICATOR_H_
#define COMMUNICATOR_H_

struct Communicator {
protected:

    using Network = brick::net::Network;
    using Message = brick::net::Message;
    using BaseSocket = Connections::BaseSocket;
    using Socket = Connections::Socket;
    using Resolution = brick::net::Resolution< Socket >;

    enum { ALL = 255 };

public:
    Communicator( const char *port, bool autobind ) :
        _id( 0 ),
        _worldSize( 0 ),
        _net( port, autobind ),
        _listenerRunning( false )
    {}
    virtual ~Communicator() {
        stopListening();
        while( listenerRunning() );
    }

    bool sendTo( int id, Message &message ) {
        Socket socket = connections().find( id );
        if ( !socket )
            return false;
        message.from( _id );
        message.to( id );
        socket->send( message );
        return true;
    }

    void sendAll( Message & );

    Message receive( int = -1 );
    Message receiveFrom( int, int = -1 );

    void listen() {
        listen( []( Message & ){} );
    }

    template< typename Callback >
    void listen( Callback callback ) {
        static_assert(
            std::is_same< void, typename std::result_of< Callback( Message & ) >::type >::value,
            "callback has to have signature `void( DataContainer * )`" );

        _listen.store( true, std::memory_order_relaxed );
        _listenerRunning.store( true, std::memory_order_relaxed );

        while ( _listen.load( std::memory_order_relaxed ) ) {
            std::vector< Socket > sockets;
            loop( sockets, callback );
        }

        _listenerRunning.store( false, std::memory_order_relaxed );
    }

    void stopListening() {
        _listen.store( false, std::memory_order_relaxed );
    }

    bool master() const {
        return id() == 0;
    }
    int id() const {
        return _id;
    }
    int worldSize() const {
        return _worldSize;
    }
    const std::string &name() const {
        return _name;
    }
protected:

    void id( int i ) {
        _id = i;
    }
    void worldSize( int ws ) {
        _worldSize = ws;
    }
    void name( std::string n ) {
        _name.swap( n );
    }

    Connections &connections() {
        return _connections;
    }

    Network &net() {
        return _net;
    }

    Socket connect( const Address &address ) {
        return std::make_shared< BaseSocket >( _net.connect( address.value() ) );
    }

    static std::pair< Socket, Socket > socketPair() {
        auto original = Network::socketPair();

        return {
            std::make_shared< BaseSocket >( std::move( original.first ) ),
            std::make_shared< BaseSocket >( std::move( original.second ) )
        };
    }

    bool listenerRunning() const {
        return _listenerRunning.load( std::memory_order_relaxed );
    }

    void processHyperCube( Message & );

    void loop( std::vector< Socket > &sockets, int timeout = 1000 ) {
        loop( sockets, []( const Message & ){}, timeout );
    }

    template< typename Callback >
    void loop( std::vector< Socket > &sockets, Callback callback, int timeout = 100 ) {
        {
            std::lock_guard< std::mutex > _( connections().mutex() );
            sockets.reserve( sockets.size() + connections().size() );

            std::copy(
                connections().vbegin(),
                connections().vend(),
                std::back_inserter( sockets )
            );
        }
        Resolution r = _net.poll( sockets, timeout );

        if ( r.resolution() == Resolution::Timeout ) {
            if ( id() )
                Logger::log( id(), "R=timeout" );
            return;
        }

        if ( r.resolution() == Resolution::Incomming ) {
            if ( id() )
                Logger::log( id(), "R=incomming" );
            processIncomming( std::make_shared< BaseSocket >( _net.incomming() ) );
            return;
        }

        for ( Socket &s : r.sockets() ) {
            if ( id() )
                Logger::log( id(), "R=ready" );
            if ( s->closed() ) {
                processDisconnected( std::move( s ) );
                continue;
            }

            if ( !readSocket( std::move( s ), callback ) )
                break;
        }
    }

    bool readSocket( Socket &&socket ) {
        return readSocket( std::move( socket ), []( const Message & ) {} );
    }
    template< typename Callback >
    bool readSocket( Socket &&socket, Callback callback ) {
        Message message = socket->receive();

        if ( message.to() == ALL )
            processHyperCube( message );
        switch ( message.category< MessageType >() ) {
        case MessageType::Data:
            callback( message );
            break;
        case MessageType::Control:
            return processControl( message, std::move( socket ) );
        case MessageType::OutputStandard:
            processStandardOutput( message, std::move( socket ) );
            break;
        case MessageType::OutputError:
            processStandardError( message, std::move( socket ) );
            break;
        default:
            Logger::log( id(), "invalid header of incomming message from " + info( socket ) );
            break;
        }
        return true;
    }

    std::string info( Socket s ) {
        return s->name() + " [" + s->address().value() + "]";
    }

private:

    virtual bool processControl( Message &, Socket ) = 0;
    virtual void processDisconnected( Socket socket ) {
        connections().lockedEraseIf( [&]( const std::pair< int, Socket > &s ) -> bool {
            return socket == s.second;
        } );
    }
    virtual void processIncomming( Socket s ) {
        Logger::log( _id, "unacceptable incomming connection from " + info( s ) );
        s->close();
        throw NetworkException{ "unexpected incomming connection" };
    }
    virtual void processStandardOutput( Message &, Socket ) {}
    virtual void processStandardError( Message &, Socket ) {}


    Message processResolution( const Resolution & );

    int _id;
    int _worldSize;
    std::atomic< bool > _listen;
    std::atomic< bool > _listenerRunning;
    Connections _connections;
    Network _net;
    std::string _name;
};

#ifdef BRICK_UNITTEST_REG

namespace netTest {

using namespace brick::net;

struct Test {

    TEST(NetworkSocketPair) {
        auto pair = Network::socketPair();
        ASSERT_NEQ( pair.first.fd(), Socket::Invalid );
        ASSERT_NEQ( pair.second.fd(), Socket::Invalid );
    }
    TEST(CommunicatorSocketPair) {
        auto pair = Communicator::socketPair();
        ASSERT_NEQ( pair.first->fd(), Socket::Invalid );
        ASSERT_NEQ( pair.second->fd(), Socket::Invalid );
    }
    TEST(tie) {
        Communicator::Socket a, b;
        std::tie( a, b ) = Communicator::socketPair();
        ASSERT_NEQ( a->fd(), Socket::Invalid );
        ASSERT_NEQ( b->fd(), Socket::Invalid );

    }
};

}

#endif

#endif
