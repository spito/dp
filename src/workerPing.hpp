#include <vector>
#include <future>
#include <mutex>
#include <random>
#include <condition_variable>
#include <sstream>
#include <chrono>

#include <brick-shmem.h>

#include "daemon.h"
#include "worker.hpp"

namespace ping {

struct Timer {
    using TimePoint = std::chrono::high_resolution_clock::time_point;
    using Duration = std::chrono::high_resolution_clock::duration;

    void start() {
        _startTime = now();
    }

    void stop() {
        _stopTime = now();
    }

    static TimePoint now() {
        return std::chrono::high_resolution_clock::now();
    }

    template< typename T = Duration >
    long long elapsed() const {
        return std::chrono::duration_cast< T >( _stopTime - _startTime ).count();
    }
private:
    TimePoint _startTime;
    TimePoint _stopTime;
};

struct CumulativeTimer {
    CumulativeTimer() :
        _elapsed{ 0 }
    {}

    void reset() {
        _elapsed = 0;
    }

    long long elapsed() {
        return _elapsed;
    }

    brick::types::Defer section() {
        Timer::TimePoint start = Timer::now();
        return [this,start] {
            _elapsed += (Timer::now() - start).count();
        };
    }
private:
    long long _elapsed;
};

template< typename Self >
struct Worker : BaseWorker {

    Worker( int id, Common &common ) :
        BaseWorker{ id, common },
        _generator{ std::random_device{}() },
        _distribution{ 1, common.worldSize() - 1 },
        _seed( std::random_device{}() )
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

    static void notifyAll( Common &common ) {
        common.progress();
        brick::net::OutputMessage msg( MessageType::Data );
        msg.tag( Tag::Done );

        Daemon::instance().sendAll( msg );
    }

    static bool isMaster( Common &common ) {
        return common.rank() == Daemon::MainSlave;
    }

    static void dispatcher( Common &common, std::vector< Self > &workers ) {
        while ( common.processed() < common.worldSize() ) {
            Daemon::instance().probe( [&]( Channel channel ) {
                    Self::processDispatch( common, workers, channel );
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
    inline int owner( Package p ) {
        int o;
        switch ( common().selection() ) {
        default:
        case 1:
            o = p.hash() % ( common().worldSize() - 1 ) + 1;
        case 2:
            return common().rank() % common().worldSize() + 1;
        case 3:
            o = _distribution( _generator );
            break;
        case 4:
            o = _generator() % ( common().worldSize() - 1 ) + 1;
            break;
        case 5:
            o = ::rand_r( &_seed ) % ( common().worldSize() - 1 ) + 1;
            break;
        }

        if ( o >= common().rank() )
            ++o;
        return o;
    }

    bool quit() const {
        return processed() == common().worldSize();
    }

    static void request( int from, Package p ) {
        brick::net::OutputMessage o( MessageType::Data );
        o.tag( Tag::Response );

        p.first *= -1;

        o << p;
        Daemon::instance().sendTo( from, o, Self::channel( p.second ) );
    }

private:

    Self &self() {
        return *static_cast< Self * >( this );
    }

    std::ranlux24 _generator;
    std::uniform_int_distribution<> _distribution;
    unsigned _seed;
    std::future< void > _handle;
};


struct Box {

    Box() = default;
    Box( const Box & ) = delete;
    Box( Box && ) = delete;

    void signal( int value ) {
        std::lock_guard< std::mutex > _{ _mutex };
        _ready = true;
        _value = value;
        _cv.notify_one();
    }

    int wait() {
        std::unique_lock< std::mutex > guard{ _mutex };
        _cv.wait( guard, [this] {
             return _ready;
        } );
        _ready = false;
        return _value;
    }

private:
    bool _ready = false;
    int _value;
    std::mutex _mutex;
    std::condition_variable _cv;
};


struct Shared : Worker< Shared > {

    Shared( int id, Common &common ) :
        Worker< Shared >{ id, common },
        _box{ new Box }
    {}

    Shared( const Shared & ) = delete;
    Shared( Shared && ) = default;

    void push( int v ) {
        _box->signal( v );
    }

    static ChannelID channel( int ) {
        return ChannelType::Master;
    }

    void main() {
        for ( int i = 0; i < common().workLoad(); ++i ) {
            brick::net::OutputMessage msg( MessageType::Data );
            Package p;
            p.first = i + 13;
            p.second = id();

            msg.tag( Tag::Request );
            msg << p;
            int target = owner( p );

            if ( !Daemon::instance().sendTo( target, msg ) )
                std::cout << target << "fail" << std::endl;
            else {
                int response = _box->wait();

                if ( response != -p.first )
                    std::cout << response << " != " << -p.first << std::endl;
            }
        }
    }

    static void processDispatch( Common &common, std::vector< Shared > &workers, Channel channel ) {
        Package p;
        brick::net::InputMessage incoming;
        incoming >> p;
        channel->receive( incoming );

        switch ( incoming.tag< Tag >() ) {
        case Tag::Request:
            request( incoming.from(), p );
            break;
        case Tag::Response:
            workers[ p.second ].push( p.first );
            break;
        case Tag::Done:
            common.progress();
            break;
        default:
            break;
        }
    }
private:
    std::unique_ptr< Box > _box;
};

struct Dedicated : Worker< Dedicated > {
    Dedicated( int id, Common &common ) :
        Worker{ id, common }
    {}

    static ChannelID channel( int ch ) {
        return ch;
    }

    void main() {
        for ( int i = 0; i < common().workLoad(); ++i ) {
            brick::net::OutputMessage msg( MessageType::Data );
            Package p;
            p.first = i + 13;
            p.second = id();

            msg.tag( Tag::Request );
            msg << p;

            int target = owner( p );
            Daemon::instance().sendTo( target, msg );

            brick::net::InputMessage input;

            Package incoming;
            input >> incoming;
            Daemon::instance().receive( target, input, channel( id() ) );

            if ( incoming.first != -p.first )
                std::cout << incoming.first << " != " << -p.first << std::endl;

        }
    }
    static void processDispatch( Common &common, std::vector< Dedicated > &, Channel channel ) {
        Package p;
        brick::net::InputMessage incoming;
        incoming >> p;
        channel->receive( incoming );

        switch ( incoming.tag< Tag >() ) {
        case Tag::Request:
            request( incoming.from(), p );
            break;
        case Tag::Done:
            common.progress();
            break;
        default:
            break;
        }
    }
};

} // namespace ping
