#include <future>
#include <queue>
#include <brick-hashset.h>

#include "daemon.h"
#include "worker.hpp"

namespace load {

template< typename Self >
struct Worker : BaseWorker {

    using BaseWorker::BaseWorker;

    void start() {
        _handle = std::async( std::launch::async, [this] {
            try {
                this->self().main();
            } catch ( const std::exception &e ) {
                std::cerr << "exception: " << e.what() << std::endl;
            }
        } );
    }
    void wait() {
        _handle.get();
    }

    static void notifyAll( Common & ) {
        brick::net::OutputMessage msg( MessageType::Data );
        msg.tag( Tag::Done );

        Daemon::instance().sendAll( msg );
    }

    static bool isMaster( Common &common ) {
        return common.rank() == Daemon::MainSlave;
    }

    static void dispatcher ( Common &common, const std::vector< Self > & ) {
        while ( !common.quit() ) {
            Daemon::instance().probe( [&]( Channel channel ) {
                    Self::processDispatch( common, channel );
                },
                ChannelType::Master,
                10000
            );
        }
    }

    static int rank() {
        return Daemon::instance().id();
    }
    static int worldSize() {
        return Daemon::instance().worldSize();
    }

protected:
    unsigned owner( Package p ) {
        return p.hash() % common().worldSize() + 1;
    }

    template< typename Yield >
    void successors( Package p, Yield yield ) {
        if ( p.first < common().workLoad() ) {
            Package s = p;
            s.result = F();
            s.first++;
            yield( s );
        }
        if ( p.second < common().workLoad() ) {
            Package s = p;
            s.result = F();
            s.second++;
            yield( s );
        }
    }

    void process( Package p, ChannelID chID ) {
        progress();

        if ( p.first == common().workLoad() && p.second == common().workLoad() ) {
            done();
            return;
        }
        expand( p, chID );
    }

    void expand( Package p, ChannelID chID ) {
        unsigned o = owner( p );
        if ( o == common().rank() ) {
            if ( withTD().insert( p ).isnew() ) {
                push( p );
            }
            return;
        }
        brick::net::OutputMessage msg( MessageType::Data );
        msg.tag( Tag::Data );
        msg << p;
        Daemon::instance().sendTo( o, msg, chID );
    }

private:
    Self &self() {
        return *static_cast< Self * >( this );
    }

    std::future< void > _handle;
};


struct Shared : Worker< Shared > {
    using Worker< Shared >::Worker;

    void main() {

        while ( !quit() ) {
            Package p;
            if ( pop( p ) ) {
                successors( p, [this]( Package n ) {
                    process( n, ChannelType::Master );
                } );
            }
        }
        done();
    }

    static void processDispatch( Common &common, Channel channel ) {
        static Set::ThreadData td;
        brick::net::InputMessage incoming;
        Package p;
        incoming >> p;
        channel->receive( incoming );

        switch( incoming.tag< Tag >() ) {
        case Tag::Data:
            if ( common.withTD( td ).insert( p ).isnew() )
                common.push( p );
            break;
        case Tag::Done:
            common.done();
            break;
        default:
            break;
        }
    }
};

struct Dedicated : Worker< Dedicated > {
    using Worker< Dedicated >::Worker;

    void main() {

        while ( !quit() ) {
            Package p;
            if ( pop( p ) ) {
                successors( p, [this]( Package p ) {
                    process( p, id() );
                } );
            }
            Daemon::instance().probe( [&,this]( Channel channel ) {
                    receive( channel );
                },
                id(),
                0
            );
        }
    }

    void receive( Channel channel ) {
        brick::net::InputMessage incoming;
        Package p;
        incoming >> p;
        channel->receive( incoming );
        expand( p, id() );
    }

    static void processDispatch( Common &common, Channel channel ) {
        brick::net::InputMessage incoming;
        channel->receiveHeader( incoming );
        if ( incoming.tag< Tag >() == Tag::Done )
            common.done();
    }
};


}
