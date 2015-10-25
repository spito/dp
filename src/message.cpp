#include "message.h"

const char *codeToString( Code code ) {
    switch( code ) {
    case Code::ACK:
        return "ACK";
    case Code::SUCCESS:
        return "SUCCESS";
    case Code::FAILED:
        return "FAILED";
    case Code::REFUSE:
        return "REFUSE";
    case Code::ENSLAVE:
        return "ENSLAVE";
    case Code::ID: // %D
        return "ID";
    case Code::CONNECT: // %S %D
        return "CONNECT";
    case Code::JOIN:
        return "JOIN";
    case Code::DISCONNECT:
        return "DISCONNECT";
    case Code::PEERS: // %D
        return "PEERS";
    case Code::LEAVE:
        return "LEAVE";
    case Code::GROUPED:
        return "GROUPED";
    case Code::PREPARE:
        return "PREPARE";
    case Code::SHUTDOWN:
        return "SHUTDOWN";
    case Code::CUTROPE:
        return "CUTROPE";
    case Code::ERROR:
        return "ERROR";
    case Code::RENEGADE:
        return "RENEGADE";

#ifdef DEBUG
    case Code::TABLE: // debug only
        return "TABLE";
#endif
    default:
        return "?unknown code?";
    }
}
