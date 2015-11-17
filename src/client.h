//#include <signal.h>
#include <unordered_map>
#include <string>

#include "message.h"
#include "communicator.h"

#ifndef CLIENT_H
#define CLIENT_H

struct Client : Communicator {

    Client( const char *port, int channels = 1 ) :
        Communicator( port, false ),
        _channels( channels )
    {}

    ~Client() {
        quit();
    }

    // the only thread-safe method
    void quit();
    bool shutdown( const std::string & );
    void forceShutdown( const std::string & );

    bool add( std::string );
    bool removeAll();

    void list();
    void table();

    void run( int, char **, const void * = nullptr, size_t = 0);

private:

    bool establish( int, char **, const void *, size_t );
    bool dissolve();

    bool initWorld();
    bool connectAll();
    bool start( int, char **, const void *, size_t );

    void reset();
    void reset( Address );
    bool processControl( Channel ) override;
    void processDisconnected( Channel ) override;
    void processOutput( Channel ) override;

    bool done();
    void error( Channel );
    void renegade( InputMessage &, Channel );

    void refreshCache();

    int _idCounter = 1;
    int _done = 0;
    int _channels;
    bool _quit = false;
    bool _established = false;

    std::unordered_map< std::string, int > _names;
    std::vector< Channel > _cache;

};

#endif
