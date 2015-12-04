#include <vector>
#include <future>
#include <mutex>
#include <random>
#include <condition_variable>
#include <sstream>
#include <chrono>

#include <brick-shmem.h>

#include "daemon.h"

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

enum class DataType {
    Request,
    Response,
    Done
};

template< typename Self >
struct Worker {
    Worker( int id, int workLoad, int selecting ) :
        _id{ id },
        _workLoad{ workLoad },
        _selecting{ selecting },
        _generator{ std::random_device{}() },
        _distribution{ 1, Daemon::instance().worldSize() - 1 },
        _seed( std::random_device{}() )
    {}

    int id() const {
        return _id;
    }
    int workLoad() const {
        return _workLoad;
    }

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
protected:
    inline int recepient( int i ) {
        auto _{ _tm.section() };
        if ( _selecting == 1 ) {
            return Daemon::instance().id() % Daemon::instance().worldSize() + 1;
        }
        if ( _selecting == 2 ) {
            i %= Daemon::instance().worldSize() - 1;
            ++i;
            if ( i >= Daemon::instance().id() )
                ++i;
            return i;
        }

        int number;
        if ( _selecting == 4 )
            number = _generator() % ( Daemon::instance().worldSize() - 1 ) + 1;
        else if ( _selecting == 5 )
            number = ::rand_r( &_seed ) % ( Daemon::instance().worldSize() - 1 ) + 1;
        else
            number = _distribution( _generator );

        if ( number >= Daemon::instance().id() )
            ++number;
        return number;
    }

    void done( int i ) {
//        if ( workLoad() >= 10 && (i + 1) % ( workLoad() / 10 ) == 0 )
//            std::cout << "|" << std::flush;
        if ( i + 1 == workLoad() )
            std::cout << "[" << _tm.elapsed() << "]" << std::endl;
    }
private:

    Self &self() {
        return *static_cast< Self * >( this );
    }

    int _id;
    int _workLoad;
    int _selecting;
    std::ranlux24 _generator;
    std::uniform_int_distribution<> _distribution;
    unsigned _seed;
    std::future< void > _handle;
    CumulativeTimer _tm;
};


struct Package {

    Package() = default;
    Package( const Package & ) = delete;
    Package( Package && ) = delete;

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

    Shared( int id, int workLoad, int selecting ) :
        Worker< Shared >{ id, workLoad, selecting },
        _package{ new Package }
    {}

    Shared( const Shared & ) = delete;
    Shared( Shared && ) = default;

    void push( int v ) {
        _package->signal( v );
    }

    static ChannelID channel( int ) {
        return ChannelType::Master;
    }

    void main() {
        for ( int i = 0; i < workLoad(); ++i ) {
            brick::net::OutputMessage msg( MessageType::Data );
            int data = i + 13;
            msg.tag( DataType::Request );
            msg << id() << data;
            int target = recepient( data );

            // std::ostringstream out;
            // out << Daemon::instance().id() << "."
            //     << id() <<" --[" << data << "]--> "
            //     << target << std::endl;
            // std::cout << out.str() << std::flush;


            if ( !Daemon::instance().sendTo( target, msg ) )
                std::cout << "fail" << std::endl;
            else {
                int response = _package->wait();

                if ( response != -data )
                    std::cout << response << " != " << -data << std::endl;
            }
            done( i );
        }
    }
private:
    std::unique_ptr< Package > _package;
};

struct Dedicated : Worker< Dedicated > {
    Dedicated( int id, int workLoad, int selecting ) :
        Worker< Dedicated >{ id, workLoad, selecting }
    {}

    void push( int ) {
        std::cout << "error" << std::endl;
    }
    static ChannelID channel( int ch ) {
        return ch;
    }

    void main() {
        for ( int i = 0; i < workLoad(); ++i ) {
            brick::net::OutputMessage msg( MessageType::Data );
            int data = i + 13;
            msg.tag( DataType::Request );
            msg << id() << data;

            int target = recepient( data );
            Daemon::instance().sendTo( target, msg );

            brick::net::InputMessage input;
            int response;
            int ignore;

            input >> ignore >> response;
            Daemon::instance().receive( target, input, channel( id() ) );

            if ( response != -data )
                std::cout << response << " != " << -data << std::endl;

            done( i );
        }
    }

};

template< typename W >
struct Workers {

    Workers( int workers, int workLoad, int selecting ) :
        _done( 0 )
    {
        _workers.reserve( workers );
        for ( int i = 0; i < workers; ++i ) {
            _workers.emplace_back( i, workLoad, selecting );
        }

    }

    void run() {
        for ( auto &w : _workers )
            w.start();

        std::future< void > d = std::async( std::launch::async, [this] {
            this->dispatcher();
        } );

        for ( auto &w : _workers )
            w.wait();

        ++_done;
        brick::net::OutputMessage msg( MessageType::Data );
        msg.tag( DataType::Done );

        Daemon::instance().sendAll( msg );
        d.get();
    }

private:

    void dispatcher() {
        _done = 0;
        int events = 0;
        while ( _done < Daemon::instance().worldSize() ) {
            Daemon::instance().probe( [&,this]( Channel channel ) {
                    ++events;
                    this->process( channel );
                },
                ChannelType::Master,
                100
            );
        }
        // std::ostringstream out;
        // out << Daemon::instance().id() << ".dispatcher events: " << events << std::endl;
        // std::cout << out.str() << std::flush;
    }

    void process( Channel channel ) {
        int value;
        int workerId;
        brick::net::InputMessage incoming;
        incoming >> workerId >> value;
        channel->receive( incoming );

        switch ( incoming.tag< DataType >() ) {
        case DataType::Request:
            request( incoming.from(), workerId, value );
            break;
        case DataType::Response:
            _workers[ workerId ].push( value );
            break;
        case DataType::Done:
            ++_done;
            break;
        }
    }

    void request( int from, int workerId, int value ) {
        brick::net::OutputMessage o( MessageType::Data );
        o.tag( DataType::Response );

        value *= -1;

        o << workerId << value;
        // std::ostringstream out;
        // out << Daemon::instance().id() << ".dispatcher --["
        //     << value << "]--> " << from
        //     << " (" << W::channel( workerId )
        //     << ")" << std::endl;
        // std::cout << out.str() << std::flush;
        Daemon::instance().sendTo( from, o, W::channel( workerId ) );
    }

    int _final;
    std::atomic< int > _done;
    std::vector< W > _workers;
};

} // namespace ping