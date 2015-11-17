#include "message.h"

const char *codeToString( Code code ) {
    switch( code ) {
    case Code::NoOp:
        return "No";
    case Code::OK:
        return "OK";
    case Code::Success:
        return "Success";
    case Code::Refuse:
        return "Refuse";
    case Code::Enslave:
        return "Enslave";
    case Code::ID: // %D
        return "ID";
    case Code::ConnectTo: // %S %D
        return "ConnectTo";
    case Code::DataLine: // %D
        return "DataLine";
    case Code::Join:
        return "Join";
    case Code::Disconnect:
        return "Disconnect";
    case Code::Peers: // %D
        return "Peers";
    case Code::Leave:
        return "Leave";
    case Code::Grouped:
        return "Grouped";
    case Code::PrepareToLeave:
        return "PrepareToLeave";
    case Code::Shutdown:
        return "Shutdown";
    case Code::CutRope:
        return "CutRope";
    case Code::InitialData:
        return "InitialData";
    case Code::Run:
        return "Run";
    case Code::Start:
        return "Start";
    case Code::Done:
        return "Done";
    case Code::Error:
        return "Error";
    case Code::Renegade:
        return "Renegade";
    case Code::Table: // debug only
        return "Table";
    default:
        return "<unknown code>";
    }
}
