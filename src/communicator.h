#include <type_traits>
#include <chrono>
#include <algorithm>

#include <brick-net.h>
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
    using Resolution = brick::net::Resolution< Channel >;

    enum { ALL = 255 };
public:

    Communicator( const char *port, bool autobind ) :
        _id( 0 ),
        _worldSize( 0 ),
        _net( port, autobind )
    {}
    virtual ~Communicator() = default;

    bool sendTo( int id, OutputMessage &message, ChannelID chID = ChannelType::Master ) {
        return sendTo( findChannel( id, chID ), message );
    }
    bool sendTo( Channel channel, OutputMessage &message ) {
        if ( !channel || !*channel )
            return false;
        message.from( _id );
        message.to( channel->id() );
        channel->send( message );
        return true;
    }

    void sendAll( OutputMessage &, ChannelID = ChannelType::Master );

    bool receive( int id, InputMessage &message, ChannelID chID = ChannelType::Master ) {
        return receive( findChannel( id, chID ), message );
    }
    bool receive( Channel channel, InputMessage &message ) {
        if ( !channel )
            return false;
        channel->receive( message );
        return true;
    }

    template< typename Al, typename D, typename C, typename Ap >
    bool receive( int id, Al allocator, D deallocator, C convertor, Ap applicator, ChannelID chID = ChannelType::Master ) {
        Channel channel = findChannel( id, chID );
        if ( !channel )
            return false;

        InputMessage message;
        std::vector< typename std::result_of< Al( size_t ) >::type > handles;

        channel->receive( message, [&] ( size_t size ) -> char * {
            handles.emplace_back( allocator( size ) );
            return convertor( handles.back() );
        } );

        for ( auto &h : handles )
            applicator( h );

        message.cleanup< char >( deallocator );
        return true;
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

    Channel findChannel( int id, ChannelID chID ) {
        Line peer = connections().find( id );
        Channel channel;
        if ( !peer )
            return channel;
        switch ( chID.asType() ) {
        case ChannelType::Master:
            if ( peer->masterChannel() )
                channel = peer->master();
            break;
        case ChannelType::All:
            break;
        default:
            if ( peer->dataChannel( chID ) )
                channel = peer->data( chID );
            break;
        }
        return channel;
    }

    void id( int i ) {
        _id = i;
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
        Socket s = _net.connect( address.value(), noThrow );
        if ( s )
            return std::make_shared< Socket >( std::move( s ) );
        return Channel{};
    }

    std::string info( Channel channel ) {
        Line line = connections().lockedFind( channel->id() );
        if ( line )
            return info( line );
        return '#' + std::to_string( channel->id() );
    }
    std::string info( Line line ) {
        if ( !line )
            return "unknown line";
        return
            '#' + std::to_string( line->id() ) +
            ' ' + line->name() +
            " [" + line->address().value() + "]";
    }

    template< typename Ap >
    void probe( std::vector< Channel > &channels, Ap applicator, int timeout, bool listen ) {
        do {
            Resolution r = _net.poll( channels, listen, timeout );

            if ( r.resolution() == Resolution::Timeout )
                return;

            if ( listen && r.resolution() == Resolution::Incoming ) {
                processIncoming( std::make_shared< Socket >( _net.incoming() ) );
                continue;
            }

            for ( Channel &channel : r.sockets() ) {
                if ( channel->closed() ) {
                    processDisconnected( std::move( channel ) );
                    continue;
                }
                InputMessage message;
                channel->peek( message );

                bool performBreak = false;
                switch ( message.category< MessageType >() ) {
                case MessageType::Data:
                    if ( !takeBool( applicator, std::move( channel ) ) )
                        performBreak = true;
                    break;
                case MessageType::Control:
                    if ( !processControl( std::move( channel ) ) )
                        performBreak = true;
                    break;
                case MessageType::Output:
                    processOutput( std::move( channel ) );
                    break;
                default:
                    channel->receiveHeader( message );
                    break;
                }
                if ( performBreak )
                    break;
            }
        } while ( false );
    }

private:

    virtual bool processControl( Channel ) = 0;
    virtual void processOutput( Channel channel ) {
        InputMessage message;
        channel->receiveHeader( message );
    }
    virtual void processDisconnected( Channel channel ) {
        connections().lockedErase( channel->id() );
    }
    virtual void processIncoming( Channel channel ) {
        Logger::log( "unacceptable incoming connection from " + info( channel ) );
        channel->close();
        throw NetworkException{ "unexpected incoming connection" };
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


    int _id;
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
