//#include <signal.h>
#include <unordered_map>
#include <string>

#include "message.h"
#include "communicator.h"

#ifndef CLIENT_H
#define CLIENT_H

class Client : public Communicator {
    using InputMessage = brick::net::InputMessage;
    using OutputMessage = brick::net::OutputMessage;

public:
    Client( const char *port, int channels = 1 ) :
        Communicator( port, false )
    {
        this->channels( channels );
    }

    ~Client() {
        quit();
    }

    void quit();
    bool shutdown( const std::string & );
    void forceShutdown( const std::string & );
    void forceReset( const std::string & );
    std::vector< bool > start( const std::vector< std::string > & );

    bool add( std::string, std::string * = nullptr );
    bool removeAll();

    std::string status( const std::string & );

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
    static void discardMessage( Channel );

    void registerSignalHandler();
    static void handleSignal( int );

    int _idCounter = 1;
    int _done = 0;
    static int _signal;
    bool _quit = false;
    bool _established = false;

    static Channel _flag;
    Channel _watchDog;

    std::unordered_map< std::string, int > _names;
    std::vector< Channel > _cache;

};

#endif
