//#include "network.h"
#include "client.h"
#include "daemon.h"

#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <thread>

#include "worker.h"

const char *port = "41813";

#if 0
void dummyMain() {
    Daemon &d = Daemon::instance();
    std::cout << "id: " << d.id() << std::endl;
    std::cout << "data: " << ( d.data() ? "yes" : "no" ) << std::endl;


    if ( d.id() == 1 ) {
        int time = 1259;
        brick::net::OutputMessage m( MessageType::Data );
        m << time;
        bool b = d.sendTo( 2, m );
        std::cout << (b ? "ok" : "fail") << std::endl;
    }
    if ( d.id() == 2 ) {
        int time;
        d.receive(
            1,
            [&] ( size_t ) { return &time; },
            [] ( char * ) {},
            [] ( int *i ) { return reinterpret_cast< char * >( i ); },
            [] ( int * ) {}
        );
        std::cout << "time: " << time << std::endl;
    }
}
#endif

void run( const std::string &port, int argc, char **argv ) {
    try {
        Client c( port.c_str() );
        bool ok = true;
        for ( int i = 2; ok && i < argc; ++i )
            ok = c.add( argv[ i ] );
        if ( ok )
            c.run( argc, argv );
        else
            std::cerr << "cannot connect to all peers" << std::endl;
    } catch ( NetworkException &e ) {
        std::cerr << "net exception: " << e.what() << std::endl;
    } catch ( std::exception &e ) {
        std::cerr << "exception: " << e.what() << std::endl;
    } catch ( ... ) {
        std::cerr << "failed" << std::endl;
    }
}

void shutdown( const std::string &port, int argc, char **argv ) {
    Client c( port.c_str() );
    for ( int i = 2; i < argc; ++i ) {
        std::cout << argv[ i ] << ": ";
        if ( c.shutdown( argv[ i ] ) )
            std::cout << "shutdown";
        else
            std::cout << "refused";
        std::cout << std::endl;
    }
}

void forceShutdown( const std::string &port, int argc, char **argv ) {
    Client c( port.c_str() );
    for ( int i = 2; i < argc; ++i ) {
        c.forceShutdown( argv[ i ] );
    }
}

int start( int argc, char **argv ) {

    Workers w( 4, 1000000 );
    w.run();

    return 0;
}
// TODO add restart service
// TODO add start service
int main( int argc, char **argv ) {

    std::ifstream config( "net.cfg" );
    std::string port = ::port;
    config >> port;

    try {
        if ( argc >= 2 && *argv[ 1 ] == 'd' ) {
            Daemon d( port.c_str(), &start, argc >= 3 ? argv[2] : "" );
            d.run();
        }
        else if ( argc > 1 && *argv[ 1 ] == 'c' ) {
            run( port, argc, argv );
        }
        else if ( argc > 1 && *argv[ 1 ] == 'q' ) {
            forceShutdown( port, argc, argv );
        }
        else if ( argc > 1 && *argv[ 1 ] == 's' ) {
            shutdown( port, argc, argv );
        }
        else {
            //std::ofstream out( "out.txt", std::ios::app );
            std::cerr << "unsupported parametres" << std::endl;
        }
//    } catch ( brick::net::NetException &e ) {
//        std::cerr << "net exception: " << e.what() << std::endl;
//        throw;// rethrow exceptions to be catched by network layer
    } catch ( std::exception &e ) {
        std::cerr << "\terror: " << e.what() << std::endl;
    } catch ( ... ) {
        std::cerr << "failed" << std::endl;
    }
}