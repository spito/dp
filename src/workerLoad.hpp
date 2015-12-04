#include <future>
#include <brick-hashset.h>

#include "daemon.h"

namespace load {

enum class Tag {
    Data,
    Done
};

struct Package {
    unsigned first;
    unsigned second;
    unsigned result;

    Package() :
        first{ 0 },
        second{ 0 }
    {}

    unsigned hash() const {
        return (first + 7) * (second + 13);
    }
};

using brick::hash::hash128_t;
struct Hasher {
    hash128_t hash( Package p ) const {
        return { p.hash(), p.hash() };
    }
    bool valid( Package ) const {
        return true;
    }
    bool equal( Package lhs, Package rhs ) const {
        return
            lhs.first == rhs.first &&
            lhs.second == rhs.second;
    }
};

using Set = brick::hashset::FastConcurrent< Package, Hasher >;

struct Common {
    Common() :
        _done{ false },
        _processed{ 0u }
    {
        _set.setSize( 1024 );
    }
    void push( Package p ) {
        std::lock_guard< std::mutex > _( _m );
        _queue.push( p );
    }
    bool pop( Package &p ) {
        std::lock_guard< std::mutex > _( _m );
        if ( _queue.empty() )
            return false;
        p = _queue.front();
        _queue.pop();
        return true;
    }
    bool quit() const {
        return _done;
    }
    void done() {
        _done = true;
    }
    void process() {
        ++_processed;
    }
    unsigned processed() const {
        return _processed;
    }

    Set::WithTD withTD( Set::ThreadData &td ) {
        return _set.withTD( td );
    }
private:
    std::mutex _m;
    std::queue< Package > _queue;
    std::atomic< bool > _done;
    std::atomic< unsigned > _processed;
    Set _set;
};


template< typename Self >
struct BaseWorker {

    BaseWorker( int id, int workLoad, Common &common ) :
        _id{ id },
        _workLoad{ workLoad },
        _common{ common }
    {}

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

    int id() const {
        return _id;
    }
    int workLoad() const {
        return _workLoad;
    }
    static void notifyAll() {
        brick::net::OutputMessage msg( MessageType::Data );
        msg.tag( Tag::Done );

        Daemon::instance().sendAll( msg );
    }
protected:
    unsigned owner( Package p ) {
        return p.hash() % Daemon::instance().worldSize() + 1;
    }

    template< typename Yield >
    void successors( Package p, Yield yield ) {
        if ( p.first < workLoad() ) {
            Package s = p;
            s.result = F();
            s.first++;
            yield( s );
        }
        if ( p.second < workLoad() ) {
            Package s = p;
            s.result = F();
            s.second++;
            yield( s );
        }
    }

    void process( Package p, ChannelID chID ) {
        _common.process();

        if ( p.first == workLoad() && p.second == workLoad() ) {
            // std::cout << "done" << std::endl;
            done();
            return;
        }
        expand( p, chID );
    }

    void expand( Package p, ChannelID chID ) {
        unsigned o = owner( p );
        if ( o == Daemon::instance().id() ) {
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

    void push( Package p ) {
        _common.push( p );
    }
    bool pop( Package &p ) {
        return _common.pop( p );
    }
    bool quit() const {
        return _common.quit();
    }
    void done() {
        _common.done();
    }
    unsigned processed() const {
        return _common.processed();
    }
    Set::WithTD withTD() {
        return _common.withTD( _td );
    }
private:
    Self &self() {
        return *static_cast< Self * >( this );
    }

    static int F( int n = 25 ) {
        if ( n == 0 )
            return 0;
        if ( n == 1 )
            return 1;
        return F( n - 1 ) + F( n - 2 );
    }

    int _id;
    int _workLoad;
    std::future< void > _handle;
    Common &_common;
    Set::ThreadData _td;
};


struct Shared : BaseWorker< Shared > {
    using BaseWorker< Shared >::BaseWorker;

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

    static void dispatcher ( Common &common ) {
        auto timePoint = std::chrono::steady_clock::now();
        while ( !common.quit() ) {

            Daemon::instance().probe( [&]( Channel channel ) {
                    processDispatch( common, channel );
                },
                ChannelType::Master,
                1000
            );
            auto now = std::chrono::steady_clock::now();
            if ( std::chrono::duration< double >( now - timePoint ).count() >= 10.0 ) {
                // std::cout << Daemon::instance().id() << ": " << common.processed()
                //     << std::endl;
                timePoint = now;
            }
        }
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
        }
    }
};

struct Dedicated : BaseWorker< Dedicated > {
    using BaseWorker< Dedicated >::BaseWorker;

    void main() {

        while ( !quit() ) {
            Package p;
            if ( pop( p ) ) {
                successors( p, [this]( Package p ) {
                    process( p, id() );
                } );
            }
            // bool taken = true;
            // for ( int i = 0; i < 10 && taken; ++i ) {
            //     taken = false;
                Daemon::instance().probe( [&,this]( Channel channel ) {
                        // taken = true;
                        receive( channel );
                    },
                    id(),
                    0
                );
            // }
        }
    }

    void receive( Channel channel ) {
        brick::net::InputMessage incoming;
        Package p;
        incoming >> p;
        channel->receive( incoming );
        expand( p, id() );
        // if ( !withTD().insert( p ).isnew() )
        //     return;
        // std::cout << "[" << p.first << ";" << p.second << "]"<< std::endl;
        // successors( p, [this]( Package p ) {
        //     process( p, id() );
        // } );
    }

    static void dispatcher ( Common &common ) {
        auto timePoint = std::chrono::steady_clock::now();
        while ( !common.quit() ) {
            Daemon::instance().probe( [&]( Channel channel ) {
                    processDispatch( common, channel );
                },
                ChannelType::Master,
                10000
            );
            auto now = std::chrono::steady_clock::now();
            if ( std::chrono::duration< double >( now - timePoint ).count() >= 10.0 ) {
                // std::cout << Daemon::instance().id() << ": " << common.processed() << std::endl;
                timePoint = now;
            }
        }
    }
    static void processDispatch( Common &common, Channel channel ) {
        brick::net::InputMessage incoming;
        channel->receiveHeader( incoming );
        if ( incoming.tag< Tag >() == Tag::Done )
            common.done();
    }


};



template< typename W >
struct Workers {
    Workers( int workers, int workLoad ) {
        _workers.reserve( workers );
        for ( int i = 0; i < workers; ++i ) {
            _workers.emplace_back( i, workLoad, _common );
        }
    }

    void queueInitials() {
        if ( Daemon::instance().id() == Daemon::MainSlave )
            _common.push( {} );
    }

    void run() {
        queueInitials();
        for ( auto &w : _workers )
            w.start();

        std::future< void > d = std::async( std::launch::async, [this] {
            W::dispatcher( _common );
        } );

        for ( auto &w : _workers )
            w.wait();

        //_common.done(); it is implied by finishing one of the threads
        W::notifyAll();
        d.get();
    }
private:
    std::vector< W > _workers;
    Common _common;
};

}
