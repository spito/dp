#include <fstream>
#include <algorithm>

#include "meta.hpp"

std::string operator""_s( const char *string, size_t length ) {
    return {string, length};
}

struct View {

    View( void *ptr, size_t limit ) :
        _ptr( static_cast< char * >( ptr ) ),
        _limit( limit )
    {}

    template< typename T >
    View &set( const T &value ) {
        if ( !valid( sizeof( T ) ) )
            return *this;
        *reinterpret_cast< T * >( _ptr ) = value;
        move( sizeof( T ) );
        return *this;
    }

    View &set( const std::string &string ) {
        if ( !valid( sizeof( size_t ) ) )
            return *this;
        set( string.size() );
        std::copy( string.begin(), string.end(), _ptr );
        _ptr += string.size();
        return *this;
    }

    template< typename T >
    View &set( const std::vector< T > &vector ) {
        if ( !valid( sizeof( size_t ) ) )
            return *this;
        set( vector.size() );
        for ( const auto &i : vector )
            set( i );
        return *this;
    }

    template< typename T >
    View &get( T &value ) {
        if ( !valid( sizeof( T ) ) )
            return *this;
        value = *reinterpret_cast< T * >( _ptr );
        _ptr += sizeof( T );
        return *this;
    }

    View &get( std::string &string ) {
        size_t size = ~0;
        get( size );
        if ( !valid( size ) )
            return *this;

        string = std::string( _ptr, size );
        move( size );
        return *this;
    }

    template< typename T >
    View &get( std::vector< T > &vector ) {
        size_t size = ~0;
        get( size );

        if ( !valid( size ) )
            return *this;

        vector.resize( size );
        for ( auto &i : vector )
            get( i );
        return *this;
    }


private:

    bool valid( size_t length ) const {
        return length <= _limit;
    }

    void move( size_t step ) {
        _ptr += step;
        _limit -= step;
    }

    char *_ptr;
    size_t _limit;
};

struct BoolSwitch{
    BoolSwitch() :
        _value( false )
    {}
    void on() {
        _value = true;
    }
    explicit operator bool() {
        bool value = _value;
        _value = false;
        return value;
    }
private:
    bool _value;
};


Meta::Meta( int argc, char **argv, bool ignoreFostFile ) :
    command( Command::Run ),
    algorithm( Algorithm::None ),
    threads( 1 ),
    workLoad( 10 ),
    selection( 1 ),
    detach( true ),
    port( "41813" )
{

    BoolSwitch hf;
    BoolSwitch w;
    BoolSwitch th;
    BoolSwitch log;
    BoolSwitch sel;
    BoolSwitch p;

    for ( int i = 1; i < argc; ++i ) {

        if ( hf ) {
            if ( !ignoreFostFile )
                hostFile( argv[ i ] );
            continue;
        }
        if ( th ) {
            threads = std::stoi( argv[ i ] );
            continue;
        }
        if ( w ) {
            workLoad = std::stoi( argv[ i ] );
            continue;
        }
        if ( log ) {
            logFile = argv[ i ];
            continue;
        }
        if ( sel ) {
            selection = std::stoi( argv[ i ] );
            continue;
        }
        if ( p ) {
            port = argv[ i ];
            continue;
        }

        if ( argv[ i ] == "start"_s || argv[ i ] == "s"_s )
            command = Command::Start;
        else if ( argv[ i ] == "status"_s || argv[ i ] == "t"_s )
            command = Command::Status;
        else if ( argv[ i ] == "shutdown"_s || argv[ i ] == "q"_s )
            command = Command::Shutdown;
        else if ( argv[ i ] == "forceshutdown"_s || argv[ i ] == "k"_s )
            command = Command::ForceShutdown;
        else if ( argv[ i ] == "reset"_s || argv[ i ] == "r"_s )
            command = Command::ForceReset;
        else if ( argv[ i ] == "restart"_s || argv[ i ] == "rs"_s )
            command = Command::Restart;
        else if ( argv[ i ] == "daemon"_s || argv[ i ] == "d"_s  )
            command = Command::Daemon;
        else if ( argv[ i ] == "client"_s || argv[ i ] == "c"_s )
            command = Command::Run;
        else if ( argv[ i ] == "table"_s )
            algorithm = Algorithm::Table;
        else if ( argv[ i ] == "load"_s )
            algorithm = Algorithm::LoadShared;
        else if ( argv[ i ] == "ping"_s )
            algorithm = Algorithm::PingShared;
        else if ( argv[ i ] == "shared"_s );
        else if ( argv[ i ] == "dedicated"_s ) {
            if ( algorithm == Algorithm::LoadShared )
                algorithm = Algorithm::LoadDedicated;
            if ( algorithm == Algorithm::PingShared )
                algorithm = Algorithm::PingDedicated;
        }
        else if ( argv[ i ] == "long"_s ) {
            if ( algorithm == Algorithm::LoadDedicated )
                algorithm = Algorithm::LongLoadDedicated;
            if ( algorithm == Algorithm::LoadShared )
                algorithm = Algorithm::LongLoadShared;
        }
        else if ( argv[ i ] == "-h"_s )
            hf.on();
        else if ( argv[ i ] == "-n"_s )
            th.on();
        else if ( argv[ i ] == "-w"_s )
            w.on();
        else if ( argv[ i ] == "-l"_s )
            log.on();
        else if ( argv[ i ] == "-s"_s )
            sel.on();
        else if ( argv[ i ] == "-p"_s )
            p.on();
        else if ( argv[ i ] == "--no-detach"_s )
            detach = false;
    }

}

Meta::Meta( char *block, size_t size ) {
    View view( block, size );
    view.get( command )
        .get( algorithm )
        .get( threads )
        .get( workLoad )
        .get( selection )
        .get( detach )
        .get( port )
        .get( logFile )
        .get( hosts );
}

void Meta::hostFile( char *path ) {
    std::ifstream f( path );
    if ( !f )
        throw std::runtime_error( "hostfile does not exists" );
    std::string line;
    while ( std::getline( f, line ).good() )
        if ( !line.empty() && line.front() != '#' )
            hosts.push_back( line );
}

MetaBlock Meta::block() const {

    size_t size = 0;
    size += sizeof( command );
    size += sizeof( algorithm );
    size += sizeof( threads );
    size += sizeof( workLoad );
    size += sizeof( selection );
    size += sizeof( size_t ) + port.size();
    size += sizeof( size_t ) + logFile.size();
    size += sizeof( size_t );
    for ( const auto &host : hosts )
        size += sizeof( size_t ) + host.size();

    std::unique_ptr< char[] > block{ new char[ size ]() };

    View view( block.get(), size );

    view.set( command )
        .set( algorithm )
        .set( threads )
        .set( workLoad )
        .set( selection )
        .set( detach )
        .set( port )
        .set( logFile )
        .set( hosts );

    return { std::move( block ), size };
}
