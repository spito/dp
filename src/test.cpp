#include <brick-unittest.h>
#include <brick-net.h>

#define private public
#define protected public

#include "client.h"
#include "daemon.h"

int main( int argc, char **argv ) {


    return brick::unittest::run( argc > 1 ? argv[ 1 ] : "",
                                 argc > 2 ? argv[ 2 ] : "" );
}