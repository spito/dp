#include "logger.h"

#include <fstream>
#include <ctime>

void Logger::log( std::string record, bool erase ) {
    if ( _file.empty() )
        return;

    std::time_t t = std::time( nullptr );
    char timestamp[ 20 ] = {};
    std::strftime( timestamp, 20, "%Y-%m-%d %H:%M:%S", std::localtime( &t ) );

    std::ofstream file( _file, erase ? std::ios::out : std::ios::app );

    file << "[" << timestamp << "] ";
    if ( _child )
        file << "(child) ";
    file << record << std::endl;
}

void Logger::file( std::string address ) {
    _file = std::move( address );
    if ( !_file.empty() && _file.find_last_of( '.' ) == std::string::npos )
        _file += ".log";
    log( "logging started", true );
}

void Logger::becomeChild() {
    _child = true;
}

std::string Logger::_file;
bool Logger::_child( false );