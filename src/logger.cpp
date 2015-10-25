#include "logger.h"

#include <fstream>
#include <ctime>
#include <sstream>

void Logger::log( int id, std::string record ) {
    std::time_t t = std::time( nullptr );
    char timestamp[ 20 ] = {};
    std::strftime( timestamp, 20, "%Y-%m-%d %H:%M:%S", std::localtime( &t ) );

    std::ostringstream oss;

    oss << "problems-" << _address << "-" << id;
    if ( _child )
        oss << "-child";
    oss << ".log";

    std::ofstream file( oss.str().c_str(), std::ios::app );

    file << "[" << timestamp << "] " << record << std::endl;
}

void Logger::setAddress( std::string address ) {
    _address = std::move( address );
}

void Logger::becomeChild() {
    _child = true;
}

std::string Logger::_address( "~" );
bool Logger::_child( false );