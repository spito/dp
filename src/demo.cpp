//#include "network.h"
#include "client.h"
#include "daemon.h"
#include "meta.hpp"

#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <thread>

#include "workerPing.hpp"
#include "workerLoad.hpp"

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

void run( int argc, char **argv, const Meta &meta ) {
    try {
        Client c{ meta.port.c_str(), meta.threads };

        std::vector< std::pair< std::string, std::string > > problematic;
        for ( const auto &host : meta.hosts ) {
            std::string description;
            if ( !c.add( host, &description ) )
                problematic.emplace_back( host, description );
        }

        if ( problematic.empty() ) {
            //auto block = meta.block();
            c.run( argc, argv );
        }
        else {
            std::cerr << "cannot connect to those peers:" << std::endl;
            for ( const auto &peer : problematic )
                std::cerr
                    << '\t' << peer.first
                    << " (" << peer.second << ")"
                    << std::endl;

        }
    } catch ( NetworkException &e ) {
        std::cerr << "net exception: " << e.what() << std::endl;
    } catch ( std::exception &e ) {
        std::cerr << "exception: " << e.what() << std::endl;
    } catch ( ... ) {
        std::cerr << "failed" << std::endl;
    }
}

void shutdown( const Meta &meta ) {
    Client c{ meta.port.c_str() };

    for ( const auto &host : meta.hosts ) {
        std::cout << host << ": ";
        try {
            if ( c.shutdown( host ) )
                std::cout << "shutdown";
            else
                std::cout << "refused";
        } catch ( const std::exception &e ) {
            std::cout << e.what();
        }
        std::cout << std::endl;
    }
}

void forceShutdown( const Meta &meta ) {
    Client c{ meta.port.c_str() };

    for ( const auto &host : meta.hosts ) {
        std::cout << host << ": ";
        try {
            c.forceShutdown( host );
            std::cout << "killed";
        } catch ( const std::exception &e ) {
            std::cout << e.what();
        }
        std::cout << std::endl;
    }
}

template<
    template< typename > class W,
    typename I,
    typename... Args
>
void startWorker( Args... args ) {
    W< I > w( args... );
    w.run();
}

int mainD( int argc, char **argv ) {

    Meta meta( argc, argv, true );


    switch ( meta.algorithm ) {
    case Algorithm::LoadDedicated:
        startWorker< load::Workers, load::Dedicated >(
            meta.threads,
            meta.workLoad,
            meta.selection
        );
        break;
    case Algorithm::LoadShared:
        startWorker< load::Workers, load::Shared >(
            meta.threads,
            meta.workLoad,
            meta.selection
        );
        break;
    case Algorithm::PingDedicated:
        startWorker< ping::Workers, ping::Dedicated >(
            meta.threads,
            meta.workLoad,
            meta.selection
        );
        break;
    case Algorithm::PingShared:
        startWorker< ping::Workers, ping::Shared >(
            meta.threads,
            meta.workLoad,
            meta.selection
        );
        break;
    case Algorithm::Table:
        Daemon::instance().table();
        break;
    case Algorithm::None:
    default:
        std::cout << "unknown algorithm" << std::endl;
    }
    return 0;
}

void start( const Meta &meta ) {
    Client c{ meta.port.c_str() };
    std::vector< bool > result = c.start( meta.hosts );

    auto r = result.begin();
    auto m = meta.hosts.begin();

    for ( ; r != result.end(); ++r, ++m )
        std::cout << *m << ": " << ( *r ? "started" : "failed" ) << std::endl;
}

void status( const Meta &meta ) {
    Client c{ meta.port.c_str() };

    for ( const auto &host : meta.hosts ) {
        std::cout << host << ": ";
        std::cout << c.status( host ) << std::endl;
    }
}

int main( int argc, char **argv ) {

    try {
        Meta meta( argc, argv );

        switch ( meta.command ) {
        case Command::Start:
            start( meta );
            break;
        case Command::Status:
            status( meta );
            break;
        case Command::Shutdown:
            shutdown( meta );
            break;
        case Command::ForceShutdown:
            forceShutdown( meta );
            break;
        case Command::Restart:
            forceShutdown( meta );
            start( meta );
            break;
        case Command::Daemon:
            Daemon::instance( meta.port.c_str(), &mainD, meta.logFile ).run();
            break;
        case Command::Run:
            run( argc, argv, meta );
            break;
        }
    } catch ( std::exception &e ) {
        std::cerr << "\terror: " << e.what() << std::endl;
    } catch ( ... ) {
        std::cerr << "failed" << std::endl;
    }
}