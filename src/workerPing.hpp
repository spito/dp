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

template< template< typename > class S, typename Package >
struct Worker : BaseWorker< S, Package > {
    using Self = S< Package >;

    Worker( int id, Common< Package > &common ) :
        BaseWorker< S, Package >{ id, common },
        _generator{ std::random_device{}() },
        _distribution{ 1, common.worldSize() - 1 },
        _seed( std::random_device{}() )
    {}

    static void notifyAll( Common< Package > &common ) {
        common.progress();
        OutputMessage msg;
        msg.tag( Tag::Done );

        Daemon::instance().sendAll( msg );
    }

    static bool isMaster( Common< Package > &common ) {
        return common.rank() == Daemon::MainSlave;
    }

    static void dispatcher( Common< Package > &common, std::vector< Self > &workers ) {
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
        return Daemon::instance().rank();
    }
    static int worldSize() {
        return Daemon::instance().worldSize();
    }
protected:
    inline int owner( Package p ) {
        int o;
        switch ( this->common().selection() ) {
        default:
        case 1:
            o = p.hash() % ( this->common().worldSize() - 1 ) + 1;
            break;
        case 2:
            return this->common().rank() % this->common().worldSize() + 1;
        case 3:
            o = _distribution( _generator );
            break;
        case 4:
            o = _generator() % ( this->common().worldSize() - 1 ) + 1;
            break;
        case 5:
            o = ::rand_r( &_seed ) % ( this->common().worldSize() - 1 ) + 1;
            break;
        }

        if ( o >= this->common().rank() )
            ++o;
        return o;
    }

    bool quit() const {
        return this->processed() == this->common().worldSize();
    }

    static void request( int from, Package p ) {
        OutputMessage o;
        o.tag( Tag::Response );

        p.first *= -1;

        o << p;
        Daemon::instance().sendTo( from, o, Self::channel( p.second ) );
    }

private:

    std::ranlux24 _generator;
    std::uniform_int_distribution<> _distribution;
    unsigned _seed;
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

template< typename Package >
struct Shared : Worker< Shared, Package > {
    using Worker = Worker< Shared::template Shared, Package >;

    Shared( int id, Common< Package > &common ) :
        Worker{ id, common },
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
        for ( int i = 0; i < this->common().workLoad(); ++i ) {
            OutputMessage msg;
            Package p;
            p.first = i + 13;
            p.second = this->id();

            msg.tag( Tag::Request );
            msg << p;
            int target = this->owner( p );

            if ( !Daemon::instance().sendTo( target, msg ) )
                std::cout << target << " fail" << std::endl;
            else {
                int response = _box->wait();

                if ( response != -p.first )
                    std::cout << response << " != " << -p.first << std::endl;
            }
        }
    }

    static void processDispatch( Common< Package > &common, std::vector< Shared > &workers, Channel channel ) {
        Package p;
        InputMessage incoming;
        incoming >> p;
        channel->receive( incoming );

        switch ( incoming.tag< Tag >() ) {
        case Tag::Request:
            Worker::request( incoming.from(), p );
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

template< typename Package >
struct Dedicated : Worker< Dedicated, Package > {
    using Worker = Worker< Dedicated::template Dedicated, Package >;

    using Worker::Worker;

    static ChannelID channel( int ch ) {
        return ch;
    }

    void main() {
        for ( int i = 0; i < this->common().workLoad(); ++i ) {
            OutputMessage msg;
            Package p;
            p.first = i + 13;
            p.second = this->id();

            msg.tag( Tag::Request );
            msg << p;

            int target = this->owner( p );
            if ( !Daemon::instance().sendTo( target, msg ) )
                std::cout << target << " fail" << std::endl;

            InputMessage input;

            Package incoming;
            input >> incoming;
            if ( !Daemon::instance().receive( target, input, channel( this->id() ) ) )
                std::cout << target << " fail " << channel( this->id() ) << std::endl;


            if ( incoming.first != -p.first ) {
                std::cout << incoming.first << " != " << -p.first << std::endl;
            }

        }
    }
    static void processDispatch( Common< Package > &common, std::vector< Dedicated > &, Channel channel ) {
        Package p;
        InputMessage incoming;
        incoming >> p;
        channel->receive( incoming );

        switch ( incoming.tag< Tag >() ) {
        case Tag::Request:
            Worker::request( incoming.from(), p );
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
