#include <future>
#include <queue>
#include <brick-hashset.h>

#include "daemon.h"
#include "worker.hpp"

namespace load {

template< template< typename > class S, typename Package >
struct Worker : BaseWorker< S, Package > {

    using BaseWorker< S, Package >::BaseWorker;
    using Self = S< Package >;
    using Common = Common< Package >;

    static void notifyAll( Common & ) {
        OutputMessage msg;
        msg.tag( Tag::Done );

        Daemon::instance().sendAll( msg );
    }

    static bool isMaster( Common &common ) {
        return common.rank() == Daemon::MainSlave;
    }

    static void dispatcher ( Common &common, const std::vector< Self > & ) {
        Self::init( common );
        while ( !common.quit() ) {
            int p = Daemon::instance().probe( [&]( Channel channel ) {
                    Self::processDispatch( common, channel );
                },
                ChannelType::Master,
                100
            );
            if ( p == 0 )
                Self::flush();
        }
    }
    static void init( Common & ) {}
    static void flush() {}

    static int rank() {
        return Daemon::instance().rank();
    }
    static int worldSize() {
        return Daemon::instance().worldSize();
    }

protected:
    unsigned owner( Package p ) {
        return p.hash() % this->common().worldSize() + 1;
    }

    template< typename Yield >
    void successors( Package p, Yield yield ) {
        if ( p.first < this->common().workLoad() ) {
            Package s = p;
            s.result = this->F();
            s.first++;
            yield( s );
        }
        if ( p.second < this->common().workLoad() ) {
            Package s = p;
            s.result = this->F();
            s.second++;
            yield( s );
        }
    }

    void process( Package p, ChannelID chID ) {
        this->progress();

        if ( p.first == this->common().workLoad() && p.second == this->common().workLoad() ) {
            this->done();
            return;
        }
        expand( p, chID );
    }

    void expand( Package p, ChannelID chID ) {
        unsigned o = owner( p );
        if ( o == this->common().rank() ) {
            if ( this->withTD().insert( p ).isnew() ) {
                this->push( p );
            }
            return;
        }
        OutputMessage msg;
        msg.tag( Tag::Data );
        msg << p;
        Daemon::instance().sendTo( o, msg, chID );
    }
};

template< typename Package >
struct Shared : Worker< Shared, Package > {
    using Worker< Shared::template Shared, Package >::Worker;
    using Common = Common< Package >;

    void main() {

        while ( !this->quit() ) {
            Package p;
            if ( this->pop( p ) ) {
                this->successors( p, [this]( Package n ) {
                    this->process( n, ChannelType::Master );
                } );
            }
        }
    }

    static void init( Common &common ) {
        qa.reset( new QueueAccessor< Package >( common.queue() ) );
    }

    static void processDispatch( Common &common, Channel channel ) {
        static typename Set< Package >::ThreadData td;

        InputMessage incoming;
        Package p;
        incoming >> p;
        channel->receive( incoming );

        switch( incoming.tag< Tag >() ) {
        case Tag::Data:
            if ( common.withTD( td ).insert( p ).isnew() )
                qa->push( p );
            break;
        case Tag::Done:
            common.done();
            break;
        default:
            break;
        }
    }
    static void flush() {
        static int x = 0;
        if ( ++x == 100 ) {
            qa->flush();
            x = 0;
        }
    }
private:
    static std::unique_ptr< QueueAccessor< Package > > qa;
};
template< typename Package >
std::unique_ptr< QueueAccessor< Package> > Shared< Package >::qa;

template< typename Package >
struct Dedicated : Worker< Dedicated, Package > {
    using Worker< Dedicated::template Dedicated, Package >::Worker;
    using Common = Common< Package >;

    void main() {
        std::queue< Package > packages;
        while ( !this->quit() ) {
            Package p;
            if ( this->pop( p ) ) {
                this->successors( p, [this]( Package p ) {
                    this->process( p, this->id() );
                } );
            }
            Daemon::instance().probe( [&,this]( Channel channel ) {
                    InputMessage msg;
                    Package p;
                    msg >> p;
                    channel->receive( msg );
                    packages.push( p );
                },
                this->id(),
                0
            );
            while ( !packages.empty() ) {
                this->expand( packages.front(), this->id() );
                packages.pop();
            }
        }
    }

    void receive( Channel channel ) {
        InputMessage incoming;
        Package p;
        incoming >> p;
        channel->receive( incoming );
        this->expand( p, this->id() );
    }

    static void processDispatch( Common &common, Channel channel ) {
        InputMessage incoming;
        channel->receiveHeader( incoming );
        if ( incoming.tag< Tag >() == Tag::Done )
            common.done();
    }
};


}
