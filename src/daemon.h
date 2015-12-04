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

    void run();

    [[noreturn]]
    void exit( int = 0 );

    char *data() const {
        return _initData.get();
    }
    size_t dataSize() const {
        return _initDataLength;
    }

    template< typename Ap >
    void probe( Ap applicator, ChannelID chId = ChannelType::Master, int timeout = -1 ) {
        Communicator::probe( _cache.at( chId.asIndex() ), applicator, timeout, false );
    }
    void table();


private:
    bool daemonize();

    bool processControl( Channel ) override;
    void processDisconnected( Channel ) override;
    void processIncoming( Channel ) override;

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
    void status( Channel );
    void initData( InputMessage &, Channel );
    void runMain( InputMessage &, Channel );


    void reset();
    void reset( Address );
    void setDefault( bool = false );

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
    int ( *_main )( int, char ** );
    std::unique_ptr< char[] > _initData;
    size_t _initDataLength = 0;

    static std::unique_ptr< Daemon > _self;

    std::vector< std::vector< Channel > > _cache;
};

#endif
