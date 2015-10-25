//#include <signal.h>
#include <unordered_map>
#include <string>

#include "message.h"
#include "communicator.h"

#ifndef CLIENT_H
#define CLIENT_H

class Client : public Communicator {

    using Message = Communicator::Message;
    using Resolution = Communicator::Resolution;
    using Socket = Communicator::Socket;

public:
    Client( const char *port ) :
        Communicator( port, false )
    {}

    ~Client() {
        quit();
    }

    // the only thread-safe method
    void quit();
    bool shutdown( const std::string & );

    bool add( std::string );
    bool removeAll();

    void list();
    void table();

    bool establish( int, char **, void * = nullptr, size_t = 0 );
    bool dissolve();

private:

    bool initWorld();
    bool connectAll();
    bool start( int, char **, void *, size_t );

    void reset();
    void reset( Address );
    bool processControl( Message &, Socket ) override;
    void processDisconnected( Socket ) override;

    bool done();
    void error( Socket );
    void renegade( Message &, Socket );

    int _idCounter = 1;
    int _done = 0;
    bool _established = false;

    std::unordered_map< std::string, int > _names;

};

#endif
