#pragma once

#include <future>
#include <queue>
#include <atomic>
#include <mutex>
#include <vector>
#include <algorithm>

#include <brick-hashset.h>

enum class Tag {
    Data,
    Request,
    Response,
    Done
};

struct Package {
    int first;
    int second;
    unsigned result;

    Package() :
        first{ 0 },
        second{ 0 },
        result{ 0 }
    {}

    unsigned hash( unsigned salt = 0 ) const {
        return ( unsigned(first + 7) * unsigned(second + 13) ) ^ salt;
    }
};

struct LongPackage : Package {
    char padding[10240];
};

using brick::hash::hash128_t;

template< typename Package >
struct Hasher {
    hash128_t hash( Package &p ) const {
        return { p.hash(), p.hash( ~0u ) };
    }
    bool valid( Package & ) const {
        return true;
    }
    bool equal( Package &lhs, Package &rhs ) const {
        return
            lhs.first == rhs.first &&
            lhs.second == rhs.second;
    }
};

template< typename Package >
using Set = brick::hashset::FastConcurrent< Package, Hasher< Package > >;

template< typename Package >
using Chunk = std::queue< Package >;

template< typename Package >
using Queue = brick::shmem::LockedQueue< Chunk< Package > >;

template< typename Package >
struct QueueAccessor {
    using Chunk = Chunk< Package >;

    QueueAccessor( Queue< Package > & q ) :
        _q( q )
    {}

    bool empty() {
        if ( _incoming.empty() )
            _incoming = _q.pop();
        if ( _incoming.empty() )
            flush();
        return _incoming.empty();
    }
    void flush() {
        if ( !_outgoing.empty() ) {
            Chunk tmp;
            std::swap( tmp, _outgoing );
            _q.push( std::move( tmp ) );

            if ( _chunkSize < _maxChunkSize )
                _chunkSize = std::min( 2 * _chunkSize, _maxChunkSize );
        }
    }

    void push( Package p ) {
        _outgoing.push( p );
        if ( _outgoing.size() >= _chunkSize )
            flush();
    }

    bool pop( Package &p ) {
        if ( empty() )
            return false;
        p = _incoming.front();
        _incoming.pop();
        return true;
    }

private:
    const unsigned _maxChunkSize = 64;
    unsigned _chunkSize = 2;

    Queue< Package > &_q;
    Chunk _outgoing;
    Chunk _incoming;
};

template< typename Package >
struct Common {
    Common( int workLoad, int selection, int rank, int worldSize ) :
        _workLoad{ workLoad },
        _selection{ selection },
        _rank{ rank },
        _worldSize{ worldSize },
        _done{ false },
        _processed{ 0u }
    {
        _set.setSize( 1024 );
    }

    Queue< Package > &queue() {
        return _queue;
    }

    bool quit() const {
        return _done;
    }
    void done() {
        _done = true;
    }
    void progress() {
        ++_processed;
    }
    unsigned processed() const {
        return _processed;
    }

    typename Set< Package >::WithTD withTD( typename Set< Package >::ThreadData &td ) {
        return _set.withTD( td );
    }

    int F() const {
        return F( _selection );
    }

    int workLoad() const {
        return _workLoad;
    }
    int selection() const {
        return _selection;
    }
    int rank() const {
        return _rank;
    }
    int worldSize() const {
        return _worldSize;
    }
private:

    static int F( int n ) {
        if ( n == 0 )
            return 0;
        if ( n == 1 )
            return 1;
        return F( n - 1 ) + F( n - 2 );
    }

    int _workLoad;
    int _selection;
    int _rank;
    int _worldSize;
    Queue< Package > _queue;
    std::atomic< bool > _done;
    std::atomic< unsigned > _processed;
    Set< Package > _set;
};

template< template< typename > class S, typename Package >
struct BaseWorker {
    using Self = S< Package >;

    BaseWorker( int id, Common< Package > &common ) :
        _id{ id },
        _common( common ),
        _qa( _common.queue() )
    {}

    int id() const {
        return _id;
    }

    int workLoad() const {
        return _common.workLoad();
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

    // void notifyAll( Common & )
    // bool isMaster()
    // void dispatcher( Common &, std::vector< Self > & )
protected:
    void push( Package p ) {
        _qa.push( p );
    }
    bool pop( Package &p ) {
        return _qa.pop( p );
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
    typename Set< Package >::WithTD withTD() {
        return _common.withTD( _td );
    }
    const Common< Package > &common() {
        return _common;
    }
    int F() {
        return _common.F();
    }
    void progress() {
        _common.progress();
    }
private:
    Self &self() {
        return *static_cast< Self * >( this );
    }

    int _id;
    Common< Package > &_common;
    typename Set< Package >::ThreadData _td;
    QueueAccessor< Package > _qa;
    std::future< void > _handle;
};

template< template< typename > class WT, typename Package >
struct Workers {

    using W = WT< Package >;

    Workers( int workers, int workLoad, int selection ) :
        _common{ workLoad, selection, W::rank(), W::worldSize() }
    {
        _workers.reserve( workers );
        for ( int i = 0; i < workers; ++i ) {
            _workers.emplace_back( i, _common );
        }
    }

    void queueInitials() {
        if ( W::isMaster( _common ) ) {
            Chunk< Package > ch;
            ch.push( {} );
            _common.queue().push( std::move( ch ) );
        }
    }

    void run() {
        queueInitials();
        for ( auto &w : _workers )
            w.start();

        std::future< void > d = std::async( std::launch::async, [this] {
            W::dispatcher( _common, _workers );
        } );

        for ( auto &w : _workers )
            w.wait();

        //_common.done(); it is implied by finishing one of the threads
        W::notifyAll( _common );
        d.get();
    }
private:
    std::vector< W > _workers;
    Common< Package > _common;
};
