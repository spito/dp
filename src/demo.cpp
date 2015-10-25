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

const char *port = "41813";

void run( const std::string &port, int argc, char **argv ) {
    try {
        Client c( port.c_str() );
        for ( int i = 2; i < argc; ++i )
            c.add( argv[ i ] );
        if ( c.establish( argc, argv ) )
            c.listen();
        c.dissolve();
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
    for ( int i = 2; i < argc; ++i )
        c.shutdown( argv[ i ] );
}

int start( int argc, char **argv ) {
    Daemon &d = Daemon::instance();
    std::cout << "id: " << d.id() << std::endl;
    std::cout << "data: " << ( d.data() ? "yes" : "no" ) << std::endl;
    return 0;
}

int main( int argc, char **argv ) {

    std::ifstream config( "net.cfg" );
    std::string port = ::port;
    config >> port;

    try {
        if ( argc == 2 && *argv[ 1 ] == 'd' ) {
            Daemon d( port.c_str(), &start );
            d.run();
        }
        else if ( argc > 1 && *argv[ 1 ] == 'c' ) {
            run( port, argc, argv );
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