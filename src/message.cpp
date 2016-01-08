#include "message.h"

const char *codeToString( Code code ) {
    switch( code ) {
    case Code::OK:
        return "OK";
    case Code::Refuse:
        return "Refuse";
    case Code::Enslave:
        return "Enslave";
    case Code::Disconnect:
        return "Disconnect";
    case Code::Peers:
        return "Peers";
    case Code::ConnectTo:
        return "ConnectTo";
    case Code::Join:
        return "Join";
    case Code::DataLine:
        return "DataLine";
    case Code::Grouped:
        return "Grouped";
    case Code::InitialData:
        return "InitialData";
    case Code::Run:
        return "Run";
    case Code::Start:
        return "Start";
    case Code::Done:
        return "Done";
    case Code::PrepareToLeave:
        return "PrepareToLeave";
    case Code::Leave:
        return "Leave";
    case Code::CutRope:
        return "CutRope";
    case Code::Error:
        return "Error";
    case Code::Renegade:
        return "Renegade";
    case Code::Status:
        return "Status";
    case Code::Shutdown:
        return "Shutdown";
    case Code::ForceShutdown:
        return "ForceShutdown";
    case Code::ForceReset:
        return "ForceReset";
    default:
        return "<unknown code>";
    }
}
