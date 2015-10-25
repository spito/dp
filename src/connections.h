#include <memory>
#include <unordered_map>
#include <mutex>

#include <brick-net.h>

#include "message.h"

#ifndef CONNECTIONS__H_
#define CONNECTIONS__H_

struct Connections {

    struct BaseSocket : brick::net::Socket {

        BaseSocket( int id, Socket &&base ) :
            Socket( std::move( base ) ),
            _id( id )
        {}

        BaseSocket( Socket &&base ) :
            Socket( std::move( base ) )
        {}

        template< typename A >
        void properties( int i, std::string n, A a ) {
            id( i );
            name( std::move( n ) );
            address( std::move( a ) );
        }

        int id() const {
            return _id;
        };

        void id( int i ) {
            _id = i;
        }

        const std::string &name() const {
            return _name;
        }
        void name( std::string n ) {
            _name = std::move( n );
        }

        const Address &address() const {
            return _address;
        }
        template< typename A >
        void address( A a ) {
            _address = std::move( a );
        }

    private:
        int _id;
        std::string _name;
        Address _address;
    };
    using Socket = std::shared_ptr< BaseSocket >;

    enum { InsertFailed = -1 };

    using iterator = std::unordered_map< int, Socket >::iterator;
    using const_iterator = std::unordered_map< int, Socket >::const_iterator;

	struct key_iterator : iterator {
        using iterator::iterator;
		key_iterator( iterator self ) :
			iterator( self )
		{}

		int operator*() const {
			return iterator::operator*().first;
		}
	};

    struct value_iterator : iterator {
        using iterator::iterator;
        value_iterator( iterator self ) :
            iterator( self )
        {}
        Socket &operator*() const {
            return iterator::operator*().second;
        }

        Socket operator->() const {
            return iterator::operator->()->second;
        }
    };

    struct Keys {
        Keys( Connections &self ) :
            _self( self )
        {}
        key_iterator begin() {
            return _self.kbegin();
        }
        key_iterator end() {
            return _self.kend();
        }
    private:
        Connections &_self;
    };
    struct Values {
        Values( Connections &self ) :
            _self( self )
        {}
        value_iterator begin() {
            return _self.vbegin();
        }
        value_iterator end() {
            return _self.vend();
        }
    private:
        Connections &_self;
    };

    Connections() = default;

    Connections( const Connections & ) = delete;
    Connections( Connections &&other ) :
        _table( std::move( other._table ) )
    {}

    Connections &operator=( const Connections & ) = delete;
    Connections &operator=( Connections &&other ) {
        swap( other );
        return *this;
    }

    void swap( Connections &other ) {
        using std::swap;

        swap( _table, other._table );
    }

    void lockedSwap( Connections &other ) {
        std::lock( _mutex, other.mutex() );
        std::lock_guard< std::mutex > _( _mutex, std::adopt_lock );
        std::lock_guard< std::mutex > _o( other.mutex(), std::adopt_lock );

        swap( other );
    }

    bool insert( int id, Socket socket ) {
        return _table.emplace( id, std::move( socket ) ).second;
    }

    bool lockedInsert( int id, Socket socket ) {
        std::lock_guard< std::mutex > _( _mutex );
        return insert( id, std::move( socket ) );
    }

    Socket find( int id ) const {
        auto i = _table.find( id );
        if ( i == _table.end() )
            return Socket();
        return i->second;
    }
    Socket lockedFind( int id ) {
        std::lock_guard< std::mutex > _( _mutex );
        return find( id );
    }


    bool erase( int id ) {
        return _table.erase( id ) == 1;
    }

    bool lockedErase( int id ) {
        std::lock_guard< std::mutex > _( _mutex );
        return erase( id );
    }

    template< typename UnaryPredicate >
    void eraseIf( UnaryPredicate p ) {
        auto i = _table.begin();
        while ( i != _table.end() ) {
            if ( p( *i ) )
                i = _table.erase( i );
            else
                ++i;
        }
    }

    template< typename UnaryPredicate >
    void lockedEraseIf( UnaryPredicate p ) {
        std::lock_guard< std::mutex > _( _mutex );
        eraseIf( p );
    }

    void clear() {
        _table.clear();
    }

    void lockedClear() {
        std::lock_guard< std::mutex > _( _mutex );
        clear();
    }

    bool empty() const {
        return _table.empty();
    }

    bool lockedEmpty() {
        std::lock_guard< std::mutex > _( _mutex );
        return empty();
    }

    size_t size() const {
        return _table.size();
    }

    size_t lockedSize() {
        std::lock_guard< std::mutex > _( _mutex );
        return size();
    }

    std::mutex &mutex() {
        return _mutex;
    }

    iterator begin() {
        return _table.begin();
    }
    key_iterator kbegin() {
        return begin();
    }
    value_iterator vbegin() {
        return begin();
    }

    iterator end() {
        return _table.end();
    }
    key_iterator kend() {
        return end();
    }
    value_iterator vend() {
        return end();
    }

    Values values() {
        return Values( *this );
    }
    Keys keys() {
        return Keys( *this );
    }

private:
    std::unordered_map< int, Socket > _table;
    std::mutex _mutex;
};

inline void swap( Connections &lhs, Connections &rhs ) {
    lhs.swap( rhs );
}

#endif
