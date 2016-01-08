#include <memory>
#include <unordered_map>

#include <brick-shmem.h>

#include "communicator.h"
#include "message.h"
#include "logger.h"

#ifndef DAEMON_H
#define DAEMON_H

class Daemon : public Communicator {

    enum class State {
        Free,
        Enslaved,
        FormingGroup,
        Grouped,
        Supervising,
        Running,
        Leaving,
    };

    enum class LineType {
        Master,
        Data
    };

    enum { NoChild = -1 };

    using InputMessage = brick::net::InputMessage;
    using OutputMessage = brick::net::OutputMessage;

    Daemon( const char *, int (*)( int, char ** ), std::string );
public:

    enum { MainSlave = 1 };

    Daemon( const Daemon & ) = delete;
    Daemon &operator=( const Daemon & ) = delete;

    static Daemon &instance();
    static Daemon &instance( const char *, int(*)( int, char ** ), std::string );
    static bool hasInstance();

    void run( bool = true );

    // cannot be called during probe
    [[noreturn]]
    void exit( int = 0 );

    bool master() const {
        return rank() == MainSlave;
    }

    char *data() const {
        return _initData.get();
    }
    size_t dataSize() const {
        return _initDataLength;
    }

    template< typename Ap >
    int probe( Ap applicator, ChannelID chID = ChannelType::Master, int timeout = -1 ) {
        return Communicator::probe( _cache.at( chID.asIndex() ), applicator, timeout, false );
    }
    bool sendAll( OutputMessage &message, ChannelID chID = ChannelType::Master ) {
        return Communicator::sendAll( message, _cache.at( chID.asIndex() ) );
    }

    bool sendTo( int rank, OutputMessage &message, ChannelID chID = ChannelType::Master ) {
        return sendTo( findChannel( rank, chID ), message );
    }
    bool sendTo( Channel channel, OutputMessage &message ) {
        if ( !channel || !*channel )
            return false;
        message.from( rank() );
        message.to( channel->rank() );
        std::lock_guard< std::mutex > _{ channel->writeMutex() };
        channel->send( message );
        return true;
    }

    bool receive( int rank, InputMessage &message, ChannelID chID = ChannelType::Master ) {
        return receive( findChannel( rank, chID ), message );
    }
    bool receive( Channel channel, InputMessage &message ) {
        if ( !channel )
            return false;
        std::lock_guard< std::mutex > _{ channel->readMutex() };
        channel->receive( message );
        return true;
    }

    template< typename Al, typename D, typename C, typename Ap >
    bool receive( int rank, Al allocator, D deallocator, C convertor, Ap applicator, ChannelID chID = ChannelType::Master ) {
        Channel channel = findChannel( rank, chID );
        if ( !channel )
            return false;

        InputMessage message;
        std::vector< typename std::result_of< Al( size_t ) >::type > handles;

        {
            std::lock_guard< std::mutex > _{ channel->readMutex() };
            channel->receive( message, [&] ( size_t size ) -> char * {
                handles.emplace_back( allocator( size ) );
                return convertor( handles.back() );
            } );
        }
        for ( auto &h : handles )
            applicator( h );

        message.cleanup< char >( deallocator );
        return true;
    }


    void table();

private:

    Channel findChannel( int rank, ChannelID chID ) {
        if ( chID.asType() == ChannelType::All )
            return Channel{};

        if ( rank > this->rank() )
            --rank;

        // master is not included in data channels
        if ( chID.asType() != ChannelType::Master )
            --rank;

        if ( chID.asIndex() >= _cache.size() )
            return Channel{};
        auto &peers = _cache[ chID.asIndex() ];
        if ( rank >= peers.size() )
            return Channel{};

        return peers[ rank ];
    }

    bool daemonize();
    void loop();
    void runMain();

    bool processControl( Channel ) override;
    void processDisconnected( Channel ) override;
    void processIncoming( Channel ) override;

    void enslave( InputMessage &, Channel );
    void release( Channel );
    void startGrouping( InputMessage &, Channel );
    void connecting( InputMessage &, Channel );
    void join( InputMessage &, Channel );
    void addDataLine( InputMessage &, Channel );
    void grouped( Channel );
    void initData( InputMessage &, Channel );
    void run( InputMessage &, Channel );
    void prepare( Channel );
    void leave( Channel );
    void cutRope( Channel );
    void error( Channel );
    void renegade( InputMessage &, Channel );
    void status( Channel );
    void shutdown( Channel );
    void forceShutdown();
    void forceReset();

    void reset();
    void reset( Address );
    void setDefault( bool = false );
    void waitForChild( bool );

    Channel connectLine( Address &, LineType, int = 0 );

    void becomeParent( Channel );
    void becomeChild( Channel );

    void consumeMessage( Channel );
    void redirectOutput( const char *, size_t, Output );

    void buildCache();
    const char *status() const;

    State _state;
    Channel _rope;
    int _childPid;
    bool _quit;
    bool _runMain;
    int ( *_main )( int, char ** );
    std::unique_ptr< char[] > _initData;
    std::vector< std::unique_ptr< char[] > > _arguments;
    size_t _initDataLength = 0;

    static std::unique_ptr< Daemon > _self;

    std::vector< std::vector< Channel > > _cache;
};

#endif
