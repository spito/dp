#include <type_traits>
#include <chrono>
#include <algorithm>

#include <brick-net.h>
#include <brick-types.h>
#include <brick-unittest.h>

#include "connections.h"
#include "message.h"
#include "logger.h"

#ifndef COMMUNICATOR_H_
#define COMMUNICATOR_H_

struct Communicator {
protected:

    using Network = brick::net::Network;
    using OutputMessage = brick::net::OutputMessage;
    using InputMessage = brick::net::InputMessage;

    enum { ALL = 255 };
	enum { Master = 0 };
public:

    Communicator( const char *port, bool autobind ) :
        _rank( 0 ),
        _worldSize( 0 ),
        _net( port, autobind )
    {}
    virtual ~Communicator() = default;

    int rank() const {
        return _rank;
    }
    int worldSize() const {
        return _worldSize;
    }
    int channels() const {
        return _channels;
    }
    const std::string &name() const {
        return _name;
    }

    static std::pair< Channel, Channel > socketPair() {
        auto original = Network::socketPair();

        return {
            std::make_shared< Socket >( std::move( original.first ) ),
            std::make_shared< Socket >( std::move( original.second ) )
        };
    }

protected:
    bool sendAll( OutputMessage &, ChannelID = ChannelType::Master );
    bool sendAll( OutputMessage &, std::vector< Channel > & );

    void rank( int r ) {
        _rank = r;
    }
    void worldSize( int ws ) {
        _worldSize = ws;
    }
    void channels( int c ) {
        _channels = c;
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

    Channel connect( const Address &address, bool noThrow = false ) {
        auto s = _net.connect( address.value(), noThrow );
        if ( s )
            return std::make_shared< Socket >( std::move( s ) );
        return Channel{};
    }

    std::string info( Channel channel ) {
        Line line = connections().lockedFind( channel->rank() );
        if ( line )
            return info( line );
        return '#' + std::to_string( channel->rank() );
    }
    std::string info( Line line ) {
        if ( !line )
            return "unknown line";
        return
            '#' + std::to_string( line->rank() ) +
            ' ' + line->name() +
            " [" + line->address().value() + "]";
    }

    template< typename Ap >
    int probe( std::vector< Channel > &channels, Ap applicator, int timeout, bool listen ) {

        brick::types::Defer d( [&] {
            for ( auto &channel : channels )
                channel->readMutex().unlock();
        } );

        for ( auto &channel : channels )
            channel->readMutex().lock();

        int processed = 0;
        _net.poll(
            channels,
            [this] ( brick::net::Socket s ) {
                processIncoming( std::make_shared< Socket >( std::move( s ) ) );
            },
            [&,this] ( Channel channel ) {
                return process( channel, processed, applicator );
            },
            timeout,
            listen
        );
        return processed;
    }

private:

    template< typename Ap >
    bool process( Channel channel, int &processed, Ap applicator ) {
        if ( channel->closed() ) {
            processDisconnected( std::move( channel ) );
            return true;
        }
        InputMessage message;
        channel->peek( message );

        switch ( message.category< MessageType >() ) {
        case MessageType::Data:
            ++processed;
            if ( !takeBool( applicator, std::move( channel ) ) )
                return false;
            break;
        case MessageType::Control:
            if ( !processControl( std::move( channel ) ) )
                return false;
            break;
        case MessageType::Output:
            processOutput( std::move( channel ) );
            break;
        default:
            channel->receiveHeader( message );
            break;
        }
        return true;
    }

    virtual void processIncoming( Channel channel ) {
        Logger::log( "unacceptable incoming connection from " + info( channel ) );
        channel->close();
        throw NetworkException{ "unexpected incoming connection" };
    }
    virtual bool processControl( Channel ) = 0;
    virtual void processOutput( Channel channel ) {
        InputMessage message;
        channel->receiveHeader( message );
    }
    virtual void processDisconnected( Channel channel ) {
        connections().lockedErase( channel->rank() );
    }

    template< typename Ap >
    auto takeBool( Ap applicator, Channel channel )
        -> typename std::enable_if<
            std::is_same<
                typename std::result_of< Ap(Channel) >::type,
                void
            >::value,
            bool
        >::type
    {
        applicator( std::move( channel ) );
        return true;
    }

    template< typename Ap >
    auto takeBool( Ap applicator, Channel channel )
        -> typename std::enable_if<
            std::is_same<
                typename std::result_of< Ap(Channel) >::type,
                bool
            >::value,
            bool
        >::type
    {
        return applicator( std::move( channel ) );
    }


    int _rank;
    int _worldSize;
    int _channels;
    Connections _connections;
    Network _net;
    std::string _name;
};

#ifdef BRICK_UNITTEST_REG

namespace netTest {

using namespace brick::net;

struct Test {

    using Socket = ::Socket;

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
        Channel a, b;
        std::tie( a, b ) = Communicator::socketPair();
        ASSERT_NEQ( a->fd(), Socket::Invalid );
        ASSERT_NEQ( b->fd(), Socket::Invalid );

    }
};

}

#endif

#endif
