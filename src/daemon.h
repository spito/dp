#include <memory>
#include <unordered_map>

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
        OverWatching,
        Running,
        Leaving,
    };

    enum class LineType {
        Control,
        Data
    };

    enum { NoChild = -1 };

    using InputMessage = brick::net::InputMessage;
    using OutputMessage = brick::net::OutputMessage;

public:
    Daemon( const char *port, int (*main)( int, char ** ), std::string logFile = std::string() ) :
        Communicator( port, true ),
        _state( State::Free ),
        _childPid( NoChild ),
        _quit( false ),
        _main( main )
    {
        Logger::file( std::move( logFile ) );
    }
    Daemon( const Daemon & ) = delete;
    Daemon &operator=( const Daemon & ) = delete;

    static Daemon &instance();
    static bool hasInstance();

    void run();

    [[noreturn]]
    void exit( int = 0 );

    void *data() const {
        return _initData.get();
    }
    size_t dataSize() const {
        return _initDataLength;
    }

    template< typename Ap >
    void probe( Ap applicator, ChannelID chId = ChannelType::Data, int timeout = -1 ) {
        auto i = _cache.find( chId );
        if ( i == _cache.end() )
            i = refreshCache( chId );

        Communicator::probe( i->second, applicator, false, timeout );
    }

private:

    bool processControl( Channel ) override;
    void processDisconnected( Channel ) override;
    void processIncomming( Channel ) override;

    void enslave( InputMessage &, Channel );
    void startGrouping( InputMessage &, Channel );
    void connecting( InputMessage &, Channel );
    Channel connectLine( Address &, LineType, int = 0 );
    void addDataLine( InputMessage &, Channel );
    void join( InputMessage &, Channel );
    void grouped( Channel );
    void prepare( Channel );
    void leave( Channel );
    void release( Channel );
    void cutRope( Channel );
    void shutdown( Channel );
    void forceShutdown();
    void error( Channel );
    void renegade( InputMessage &, Channel );
    void table(); // debug
    void initData( InputMessage &, Channel );
    void runMain( InputMessage &, Channel );

    void reset();
    void reset( Address );
    void setDefault();

    void becomeParent( Channel );
    void becomeChild( Channel );

    void consumeMessage( Channel );
    void redirectOutput( const char *, size_t, Output );

    auto refreshCache( ChannelID )
        -> std::unordered_map< int, std::vector< Channel > >::iterator;

    State _state;
    Channel _rope;
    int _childPid;
    bool _quit;
    int ( *_main )( int, char ** );
    std::unique_ptr< char > _initData;
    size_t _initDataLength = 0;

    static Daemon *_self;

    std::unordered_map< int, std::vector< Channel > > _cache;
};

#endif
