#include "communicator.h"

using namespace brick::net;

void Communicator::sendAll( Message &message ) {
    std::lock_guard< std::mutex > _( connections().mutex() );
    // TODO: hyper-cube
    message.from( _id );
    message.to( ALL );
    for ( auto &peer : connections() ) {
        peer.second->send( message );
    }
}

void Communicator::processHyperCube( Message &message ) {
    // TODO: hyper-cube
}

Message Communicator::receive( int timeout ) {
    std::vector< Socket > peers;
    {
        std::lock_guard< std::mutex > _( connections().mutex() );
        peers.reserve( connections().size() );
        std::copy(
            connections().vbegin(),
            connections().vend(),
            std::back_inserter( peers )
         );
    }
    Resolution r = _net.poll( peers, timeout );

    return processResolution( r );
}

Message Communicator::receiveFrom( int id, int timeout ) {
    Socket peer = connections().lockedFind( id );
    if ( !peer )
        return Message();

    if ( timeout < 0 )
        return peer->receive();

    std::vector< Socket > sources{ std::move( peer ) };
    Resolution r = _net.poll( sources, timeout );

    return processResolution( r );
}

#include <sstream>

Message Communicator::processResolution( const Resolution &r ) {
    if ( r.resolution() == Resolution::Incomming )
        throw NetworkException{ "unexpected incomming connection" };

    if ( r.resolution() == Resolution::Ready && !r.sockets().empty() )
        return r.sockets().front()->receive();

    throw WouldBlock();
}

