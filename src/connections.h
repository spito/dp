#include <memory>
#include <unordered_map>
#include <map>
#include <mutex>

#include <brick-net.h>

#include "message.h"

#ifndef CONNECTIONS__H_
#define CONNECTIONS__H_

struct Socket : brick::net::Socket {
    using Base = brick::net::Socket;

    Socket( int id, Base &&base ) :
        Base( std::move( base ) ),
        _id( id )
    {}
    Socket( Base &&base ) noexcept :
        Base( std::move( base ) )
    {}

    int id() const {
        return _id;
    }

    void id( int i ) {
        _id = i;
    }

private:
    int _id;
};

using Channel = std::shared_ptr< Socket >;

enum class ChannelType : int {

    All = -2,
    Master = -1,
    Data01 = 0, Data02, Data03, Data04, Data05, Data06, Data07, Data08, Data09, Data10,
    Data11,     Data12, Data13, Data14, Data15, Data16, Data17, Data18, Data19, Data20,
    Data21,     Data22, Data23, Data24, Data25, Data26, Data27, Data28, Data29, Data30,

    _offsetToZero = 2
};


struct ChannelID {

    ChannelID( int channel ) :
        _channel( channel )
    {}
    ChannelID( ChannelType type ) :
        _channel( static_cast< int >( type ) )
    {}

    operator int() const {
        return _channel;
    }
    ChannelType asType() const {
        return static_cast< ChannelType >( _channel );
    }
    int asIndex() const {
        return _channel + static_cast< int >( ChannelType::_offsetToZero );
    }
private:
    int _channel;
};

struct Peer {

    Peer( int id, std::string name, const char *address, Channel master, int channels = 0 ) :
        _id( id ),
        _name( std::move( name ) ),
        _address( address ),
        _master( std::move( master ) )
    {
        if ( _master )
            _master->id( _id );
        if ( channels )
            _data.reserve( channels );
    }

    Peer( Peer && ) noexcept = default;

    int id() const {
        return _id;
    };

    const std::string &name() const {
        return _name;
    }

    const Address &address() const {
        return _address;
    }
    bool dataChannel( ChannelID number ) const {
        return size_t( number ) < _data.size() && _data[ number ];
    }
    bool masterChannel() const {
        return bool( _master );
    }

    Channel master() const {
        return _master;
    }
    Channel data( ChannelID number ) const {
        if ( size_t( number ) < _data.size() )
            return _data[ number ];
        return Channel();
    }
    const std::vector< Channel > &data() const {
        return _data;
    }

    void openDataChannel( Channel channel, ChannelID number ) {
        if ( number >= _data.size() )
            _data.resize( number + 1 );
        channel->id( id() );
        _data[ number ] = std::move( channel );
    }

private:
    int _id;
    std::string _name;
    Address _address;

    Channel _master;
    std::vector< Channel > _data;
};

using Line = std::shared_ptr< Peer >;

struct Connections {

    enum { InsertFailed = -1 };
    using Table = std::map< int, Line >;

    using iterator = Table::iterator;
    using const_iterator = Table::const_iterator;

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
        Line &operator*() const {
            return iterator::operator*().second;
        }

        Line operator->() const {
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

    bool insert( int id, Line connection ) {
        return _table.emplace( id, std::move( connection ) ).second;
    }

    bool lockedInsert( int id, Line connection ) {
        std::lock_guard< std::mutex > _( _mutex );
        return insert( id, std::move( connection ) );
    }

    Line find( int id ) const {
        auto i = _table.find( id );
        if ( i == _table.end() )
            return Line();
        return i->second;
    }
    Line lockedFind( int id ) {
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
    Table _table;
    std::mutex _mutex;
};

inline void swap( Connections &lhs, Connections &rhs ) {
    lhs.swap( rhs );
}

#endif
