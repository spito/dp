#include "communicator.h"

#include <future>
#include <brick-types.h>

using namespace brick::net;

bool Communicator::sendAll( OutputMessage &message, ChannelID chID ) {

    std::vector< Channel > channels;
    {
        std::lock_guard< std::mutex > _{ connections().mutex() };
        channels.reserve( connections().size() );

        switch ( chID.asType() ) {
        case ChannelType::Master:
            for ( auto &channel : connections().values() ) {
                if ( channel->id() == id() || !channel->masterChannel() )
                    continue;
                channels.emplace_back( channel->master() );
            }
            break;
        case ChannelType::All:
            return false;
        default:
            for ( auto &channel : connections().values() ) {
                if ( channel->id() == id() || !channel->dataChannel( chID ) )
                    continue;
                channels.push_back( channel->data( chID ) );
            }
            break;
        }
    }
    return sendAll( message, channels );
}

bool Communicator::sendAll( OutputMessage &message, std::vector< Channel > &channels ) {
    message.from( _id );
    message.to( ALL );

    std::vector< std::future< bool > > handles;
    handles.reserve( channels.size() );

    brick::types::Defer d( [&] {
        for ( auto &channel : channels )
            channel->writeMutex().unlock();
    } );

    for ( auto &channel : channels )
        channel->writeMutex().lock();

    for ( auto &channel : channels ) {
        handles.emplace_back( std::async( std::launch::async, [&] {
            try {
                channel->send( message );
                return true;
            } catch ( ... ) {
                return false;
            }
        } ) );
    }
    bool result = true;
    for ( auto &h : handles )
        result = h.get() && result;
    return result;
}