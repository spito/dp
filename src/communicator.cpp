#include "communicator.h"

#include <future>

using namespace brick::net;

void Communicator::sendAll( OutputMessage &message, ChannelID chID ) {
    message.from( _id );
    message.to( ALL );

    std::vector< Channel > peers;
    std::vector< std::future< void > > handles;
    {
        std::lock_guard< std::mutex > _( connections().mutex() );
        peers.reserve( connections().size() );
        handles.reserve( connections().size() );

        switch ( chID.asType() ) {
        case ChannelType::Control:
            for ( auto &peer : connections().values() ) {
                if ( peer->id() == id() || !peer->controlChannel() )
                    continue;
                peers.emplace_back( peer->control() );
            }
            break;
        case ChannelType::DataAll:
            return;
        default:
            for ( auto &peer : connections().values() ) {
                if ( peer->id() == id() || !peer->dataChannel( chID ) )
                    continue;
                peers.push_back( peer->data( chID ) );
            }
            break;
        }
    }

    for ( auto &peer : peers ) {
        handles.emplace_back( std::async( std::launch::async, [&] {
            peer->send( message );
        } ) );
    }
    for ( auto &h : handles )
        h.get();
}

