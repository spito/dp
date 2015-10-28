#include <memory>

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

    enum class Output {
        Standard,
        Error
    };

    enum { NoChild = -1 };

public:
    Daemon( const char *port, int (*main)( int, char ** ) ) :
        Communicator( port, true ),
        _state( State::Free ),
        _childPid( NoChild ),
        _quit( false ),
        _main( main )
    {
        Logger::setAddress( net().selfAddress() );
    }
    Daemon( const Daemon & ) = delete;
    Daemon &operator=( const Daemon & ) = delete;

    static Daemon &instance();
    static bool hasInstance();

    void run();

    void *data() const {
        return _initData.get();
    }
    size_t dataSize() const {
        return _initDataLength;
    }

private:

    bool processControl( Message &, Socket ) override;
    void processDisconnected( Socket ) override;
    void processIncomming( Socket ) override;

    bool enslave( Message &, Socket );
    void startGrouping( Message &, Socket );
    bool connecting( Message &, Socket );
    bool join( Message &, Socket );
    bool grouped( Socket );
    void prepare( Socket );
    void leave( Socket );
    void release( Socket );
    void cutRope( Socket );
    void shutdown( Socket );
    void error( Socket );
    void renegade( Message & );
    void table(); // debug
    void initData( Message & );
    void runMain( Message &, Socket );

    void reset();
    void reset( Address );
    void setDefault();

    void becomeParent( Socket );
    void becomeChild( Socket );

    void redirectOutput( char *, size_t, Output );

    State _state;
    Socket _rope;
    int _childPid;
    bool _quit;
    int ( *_main )( int, char ** );
    std::unique_ptr< char > _initData;
    size_t _initDataLength = 0;

    static Daemon *_self;
};

#endif
