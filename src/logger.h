#include <string>

#ifndef LOGGER_H
#define LOGGER_H

struct Logger {

    static void log( int, std::string );
    static void setAddress( std::string );
    static void becomeChild();
private:
    static std::string _address;
    static bool _child;
};

//#ifdef DEBUG
#define NOTE() Logger::log( id(), __PRETTY_FUNCTION__ )
//#else
//#define NOTE() do {} while(false)
//#endif

#endif
