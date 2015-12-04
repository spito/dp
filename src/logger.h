#include <string>

#ifndef LOGGER_H
#define LOGGER_H

struct Logger {

    static void log( std::string text ) {
        log( std::move( text ), false );
    }
    static void file( std::string );
    static void becomeChild();
private:
    static void log( std::string, bool );
    static std::string _file;
    static bool _child;
};

//#ifdef DEBUG
#define NOTE() Logger::log( __PRETTY_FUNCTION__ )
//#else
//#define NOTE() do {} while(false)
//#endif

#endif
