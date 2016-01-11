#include <mpi.h>

#include <future>
#include <queue>
#include <vector>
#include <random>
#include <brick-hashset.h>

#include "worker.hpp"
#include "meta.hpp"

std::mutex MPI_Mutex;

template< typename Data >
struct DataPack {
    DataPack( const Data &d = Data() ) :
        _data( new Data( d ) )
    {}
    DataPack( DataPack && ) = default;
    Data *data() {
        return _data.get();
    }
    MPI_Request *request() {
        return &_request;
    }
private:
    std::unique_ptr< Data > _data;
    MPI_Request _request;
};

template< typename Data >
struct Dispatch {

    void prepare( int f, int t ) {
        _from = f;
        _tag = t;
    }

    int from() const {
        return _from;
    }
    int tag() const {
        return _tag;
    }

    void push( DataPack< Data > &&s ) {
        _requests.push( std::move( s ) );
    }

    void clearQueue( bool force = false ) {
        if ( force ) {
            while ( !_requests.empty() ) {
                MPI_Wait( _requests.front().request(), MPI_STATUS_IGNORE );
                _requests.pop();
            }
        }
        else {
            int flag;
            while ( !_requests.empty() ) {
                MPI_Test( _requests.front().request(), &flag, MPI_STATUS_IGNORE );
                if ( !flag )
                    break;
                _requests.pop();
            }
        }
    }

private:
    int _from;
    int _tag;
    std::queue< DataPack< Data > > _requests;
};

template< template< typename > class S, typename Package >
struct Worker : BaseWorker< S, Package > {

    using BaseWorker< S, Package >::BaseWorker;
    using Self = S< Package >;


    static void notifyAll( Common< Package > &common ) {
        common.progress();
        char nil = 0;
        for ( int i = 0; i < common.worldSize(); ++i ) {
            if ( i == common.rank() )
                continue;
            std::lock_guard< std::mutex > _{ MPI_Mutex };
            MPI_Send( &nil, 1, MPI_BYTE, i, int( Tag::Done ), MPI_COMM_WORLD );
        }
    }

    static bool isMaster( Common< Package > &common ) {
        return common.rank() == 0;
    }

    static int rank() {
        int r;
        MPI_Comm_rank( MPI_COMM_WORLD, &r );
        return r;
    }

    static int worldSize() {
        int ws;
        MPI_Comm_size( MPI_COMM_WORLD, &ws );
        return ws;
    }
};

template< typename Package >
struct LoadWorker : Worker< LoadWorker, Package > {
    using Worker = Worker< LoadWorker::template LoadWorker, Package >;

    using Worker::Worker;

    void main() {
        while ( !this->quit() ) {
            Package p;
            if ( this->pop( p ) ) {
                successors( p, [this]( Package n ) {
                    process( n );
                } );
            }
        }
    }

    static void dispatcher( Common< Package > &common, const std::vector< LoadWorker > & ) {
        qa.reset( new QueueAccessor< Package >( common.queue() ) );
        int x = 0;
        while ( !common.quit() ) {
            MPI_Status status;
            ::sched_yield();
            std::lock_guard< std::mutex > _{ MPI_Mutex };
            int flag;
            MPI_Iprobe( MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status );
            if ( flag ) {
                if ( status.MPI_TAG == int( Tag::Done ) ) {
                    common.done();
                    break;
                }
                processDispatch( common, status.MPI_SOURCE, status.MPI_TAG );
            }
            else {
                if ( ++x == 100 ) {
                    qa->flush();
                    x = 0;
                }
            }
        }
    }

    static void processDispatch( Common< Package > &common, int from, int tag ) {
        static typename Set< Package >::ThreadData td;

        Package p;
        MPI_Status status;
        MPI_Recv( &p, sizeof( p ), MPI_BYTE, from, tag, MPI_COMM_WORLD, &status );
        if ( common.withTD( td ).insert( p ).isnew() )
            qa->push( p );
    }

private:
    unsigned owner( Package &p ) {
        return p.hash() % this->common().worldSize();
    }

    template< typename Yield >
    void successors( Package &p, Yield yield ) {
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

    void process( Package &p ) {
        this->progress();

        if ( p.first == this->common().workLoad() && p.second == this->common().workLoad() ) {
            this->done();
            return;
        }
        expand( p );
    }

    void expand( Package &p ) {
        unsigned o = owner( p );

        if ( o == this->common().rank() ) {
            if ( this->withTD().insert( p ).isnew() ) {
                this->push( p );
            }
            return;
        }
        std::lock_guard< std::mutex > _{ MPI_Mutex };
        MPI_Send( &p, sizeof( p ), MPI_BYTE, o, int( Tag::Data ), MPI_COMM_WORLD );
    }
    static std::unique_ptr< QueueAccessor< Package > > qa;
};
template< typename Package >
std::unique_ptr< QueueAccessor< Package > > LoadWorker< Package >::qa;

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
struct PingWorker : Worker< PingWorker, Package > {
    using Worker = Worker< PingWorker::template PingWorker, Package >;

    PingWorker( int id, Common< Package > &common ) :
        Worker{ id, common },
        _generator{ std::random_device{}() },
        _distribution{ 0, common.worldSize() - 2 },
        _seed( std::random_device{}() ),
        _box{ new Box }
    {}

    void push( int v ) {
        _box->signal( v );
    }

    void main() {
        for ( int i = 0; i < this->common().workLoad(); ++i ) {
            Package p;
            p.first = i + 13;
            p.second = this->id();

            {
                std::lock_guard< std::mutex > _{ MPI_Mutex };
                MPI_Send( &p, sizeof( p ), MPI_BYTE, owner( p ), int( Tag::Request ), MPI_COMM_WORLD );
            }
            int response = _box->wait();

            if ( response != -p.first )
                std::cout << response << " != " << -p.first << std::endl;
        }
    }

    static void dispatcher( Common< Package > &common, std::vector< PingWorker > &workers ) {
        Dispatch< Package > dispatchData;
        while ( common.processed() < common.worldSize() ) {
            MPI_Status status;
            ::sched_yield();
            std::lock_guard< std::mutex > _{ MPI_Mutex };
            int flag;
            MPI_Iprobe( MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status );
            if ( flag ) {
                if ( status.MPI_TAG == int( Tag::Done ) ) {
                    common.progress();
                    char nil;
                    MPI_Recv( &nil, 1, MPI_BYTE, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE );
                    continue;
                }
                dispatchData.prepare( status.MPI_SOURCE, status.MPI_TAG );
                processDispatch( common, workers, dispatchData );
            }
            dispatchData.clearQueue();
        }
        dispatchData.clearQueue( true );
    }

    static void processDispatch( Common< Package > &common, std::vector< PingWorker > &workers, Dispatch< Package > &dispatchData ) {
        static typename Set< Package >::ThreadData td;

        Package p;
        MPI_Status status;
        MPI_Recv( &p, sizeof( p ), MPI_BYTE, dispatchData.from(), dispatchData.tag(), MPI_COMM_WORLD, &status );
        switch ( Tag( dispatchData.tag() ) ) {
        case Tag::Request:
            request( common.rank(), dispatchData, p );
            break;
        case Tag::Response:
            workers[ p.second ].push( p.first );
            break;
        default:
            break;
        }
    }
private:
    static void request( int rank, Dispatch< Package > &dispatchData, DataPack< Package > item ) {
        item.data()->first *= -1;
        MPI_Isend( item.data(), sizeof( Package ), MPI_BYTE, dispatchData.from(), int( Tag::Response ), MPI_COMM_WORLD, item.request() );
        dispatchData.push( std::move( item ) );
    }

    inline int owner( Package p ) {
        int o;
        switch ( this->common().selection() ) {
        default:
        case 1:
            o = p.hash() % ( this->common().worldSize() - 1 );
            break;
        case 2:
            return (this->common().rank() + 1) % this->common().worldSize();
        case 3:
            o = _distribution( _generator );
            break;
        case 4:
            o = _generator() % ( this->common().worldSize() - 1 );
            break;
        case 5:
            o = ::rand_r( &_seed ) % ( this->common().worldSize() - 1 );
            break;
        }

        if ( o >= this->common().rank() )
            ++o;
        return o;
    }

    std::ranlux24 _generator;
    std::uniform_int_distribution<> _distribution;
    unsigned _seed;

    std::unique_ptr< Box > _box;
};

template<
    template< typename > class W,
    typename Package
>
void startWorker( const Meta &meta ) {
    Workers< W, Package > w( meta.threads, meta.workLoad, meta.selection );
    w.run();
}

int main( int argc, char **argv ) {
    MPI_Init( &argc, &argv );

    Meta meta( argc, argv );

    switch ( meta.algorithm ) {
    case Algorithm::LoadShared:
    case Algorithm::LoadDedicated:
        startWorker< LoadWorker, Package >( meta );
        break;
    case Algorithm::LongLoadDedicated:
    case Algorithm::LongLoadShared:
        startWorker< LoadWorker, LongPackage >( meta );
        break;
    case Algorithm::PingDedicated:
    case Algorithm::PingShared:
        startWorker< PingWorker, Package >( meta );
        break;
    case Algorithm::LongPingDedicated:
    case Algorithm::LongPingShared:
        startWorker< PingWorker, LongPackage >( meta );
        break;
    default:
        break;
    }

    MPI_Finalize();
}
