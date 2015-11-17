#include <vector>
#include <future>

#include <brick-shmem.h>

#include "daemon.h"

enum class DataType {
    Request,
    Response,
    Done
};

struct Worker {

    Worker( int id, int workAmount ) :
        _id( id ),
        _workAmount( workAmount )
    {
        _handle = std::async( std::launch::async, [this] {
            this->main();
        } );
    }

    Worker( const Worker & ) = delete;
    Worker( Worker && ) = default;

    void push( int v ) {
        _queue.push( v );
    }

    void wait() {
        _handle.get();
    }

private:

    void main() {
        int ourId = Daemon::instance().id();
        int modulo = Daemon::instance().worldSize();
        int recepient = (ourId % modulo) + 1;

        for ( int i = 0; i < _workAmount; ++i ) {
            brick::net::OutputMessage msg( MessageType::Data );
            int data = i + 13;
            msg.tag( DataType::Request );
            msg << _id << data;

            Daemon::instance().sendTo( recepient, msg );

            int response = _queue.front( true );
            _queue.pop();
            if ( response != -data )
                std::cout << response << " != " << -data << std::endl;

            if ( (i + 1) % ( _workAmount / 10 ) == 0 )
                std::cout << "|" << std::flush;
        }
    }

    int _id;
    int _workAmount;
    brick::shmem::Fifo< int > _queue;
    std::future< void > _handle;
};

struct Workers {

    Workers( int workers, int workAmount ) :
        _done( 0 )
    {
        _workers.reserve( workers );
        for ( int i = 0; i < workers; ++i ) {
            _workers.emplace_back( i, workAmount );
        }

    }

    void run() {
        std::future< void > d = std::async( std::launch::async, [this] {
            this->dispatcher();
        } );
        for ( auto &t : _workers )
            t.wait();

        ++_done;
        brick::net::OutputMessage msg( MessageType::Data );
        msg.tag( DataType::Done );

        Daemon::instance().sendAll( msg );
        d.get();
    }

private:

    void dispatcher() {
        _done = 0;
        while ( _done < Daemon::instance().worldSize() ) {
            bool progress = false;
            Daemon::instance().probe( [&,this]( Channel channel ) {
                    progress = true;
                    this->process( channel );
                },
                ChannelType::Data,
                100
            );

            if ( !progress ) {
                continue;
            }
        }
    }

    void process( Channel channel ) {
        int value;
        int workerId;
        brick::net::InputMessage incomming;
        incomming >> workerId >> value;
        channel->receive( incomming );

        switch ( incomming.tag< DataType >() ) {
        case DataType::Request:
            request( incomming.from(), workerId, value );
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
        Daemon::instance().sendTo( from, o );
    }
    void response( int workerId, int value ) {
    }

    int _final;
    std::atomic< int > _done;
    std::vector< Worker > _workers;
};
