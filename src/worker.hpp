#pragma once

#include <future>
#include <queue>
#include <atomic>
#include <mutex>
#include <vector>

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

struct LongPackage {

};

using brick::hash::hash128_t;
struct Hasher {
    hash128_t hash( Package p ) const {
        return { p.hash(), p.hash( ~0u ) };
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
    void progress() {
        ++_processed;
    }
    unsigned processed() const {
        return _processed;
    }

    Set::WithTD withTD( Set::ThreadData &td ) {
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
    std::mutex _m;
    std::queue< Package > _queue;
    std::atomic< bool > _done;
    std::atomic< unsigned > _processed;
    Set _set;
};

struct BaseWorker {

    BaseWorker( int id, Common &common ) :
        _id{ id },
        _common{ common }
    {}

    int id() const {
        return _id;
    }

    int workLoad() const {
        return _common.workLoad();
    }

    // void wait()
    // void start()
    // void notifyAll( Common & )
    // bool isMaster()
    // void dispatcher( Common &, std::vector< Self > & )
protected:
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
    const Common &common() {
        return _common;
    }
    int F() {
        return _common.F();
    }
    void progress() {
        _common.progress();
    }
private:
    int _id;
    Common &_common;
    Set::ThreadData _td;
};

template< typename W >
struct Workers {
    Workers( int workers, int workLoad, int selection ) :
        _common{ workLoad, selection, W::rank(), W::worldSize() }
    {
        _workers.reserve( workers );
        for ( int i = 0; i < workers; ++i ) {
            _workers.emplace_back( i, _common );
        }
    }

    void queueInitials() {
        if ( W::isMaster( _common ) )
            _common.push( {} );
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
    Common _common;
};