#include <string>
#include <vector>

#include <brick-net.h>

#ifndef META_H
#define META_H

enum class Command {
    Start,
    Status,
    Shutdown,
    ForceShutdown,
    ForceReset,
    Restart,
    Daemon,
    Run,
};

enum class Algorithm {
    None,
    LoadShared,
    LoadDedicated,
    LongLoadShared,
    LongLoadDedicated,
    PingShared,
    PingDedicated,
    Table,
};

using MetaBlock = std::pair< std::unique_ptr< char[] >, size_t >;

struct Meta {
    Command command;
    Algorithm algorithm;
    int threads;
    int workLoad;
    int selection;
    bool detach;
    std::string port;
    std::string logFile;
    std::vector< std::string > hosts;

    Meta( int, char **, bool = false );
    Meta( char *, size_t );

    MetaBlock block() const;

private:
    void hostFile( char * );
};


#endif
