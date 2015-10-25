#include <type_traits>
#include <list>
#include <vector>
#include <unordered_map>
#include <tuple>
#include <map>
#include <mutex>
#include <condition_variable>
#include <array>
#include <atomic>

#include "brick-common.h"
#if 0
#include "brick-unittest.h"
#endif

#ifndef DATABASE_H
#define DATABASE_H

namespace brick {
namespace db {

enum NullType { Null };
enum PreventValue { Unchanged };

namespace detail {

template< typename T >
struct OptionOrValue : common::Comparable, common::Orderable {
    OptionOrValue( T &&value ) :
        xValue( std::move( value ) ),
        xOption( false )
    {}

    OptionOrValue( const T &value ) :
        xValue( value ),
        xOption( false )
    {}

    OptionOrValue( const OptionOrValue &other ) :
        xOption( other.xOption )
    {
        if ( !option() )
            xValue.create( other.value() );
    }
    OptionOrValue( OptionOrValue &&other ) :
        xOption( other.xOption )
    {
        if ( !option() )
            xValue.create( std::move( other.value() ) );
    }

    OptionOrValue() :
        xOption( true )
    {}

    ~OptionOrValue() {
        if ( !option() )
            xValue.destroy();
    }

    OptionOrValue &operator=( OptionOrValue other ) {
        swap( other );
        return *this;
    }

    void swap( OptionOrValue &other ) {
        using std::swap;

        swap( xOption, other.xOption );
        xValue.swap( other.xValue );
    }

    operator T &( ) {
        return xValue.value();
    }
    operator const T &( ) const {
        return xValue.value();
    }

    T &value() {
        return xValue.value();
    }
    const T &value() const {
        return xValue.value();
    }

    bool option() const {
        return xOption;
    }

    bool operator==( const OptionOrValue &other ) const {
        if ( xOption && other.xOption )
            return xValue.value() == other.xValue().value();
        return xOption == other.xOption;
    }

    bool operator<( const OptionOrValue &other ) const {
        return xValue.value() < other.xValue.value();
    }

private:
    union XU {
        std::array< char, sizeof( T ) > xStorage;
        T xItem;

        XU( T &&value ) :
            xItem( value )
        {}
        XU( const T &value ) :
            xItem( value )
        {}
        XU() :
            xStorage{}
        {}
        ~XU() {}
        void create( const T &value ) {
            new ( &xItem ) T( value );
        }
        void create( T &&value ) {
            new ( &xItem ) T( std::move( value ) );
        }
        void destroy() {
            xItem.~T();
        }
        T &value() {
            return xItem;
        }
        const T &value() const {
            return xItem;
        }
        void swap( XU &other ) {
            xStorage.swap( other.xStorage );
        }
    } xValue;
    bool xOption;
};

template< typename T >
struct NullOrValue : OptionOrValue< T > {

    template< typename X >
    NullOrValue( X &&value ) :
        OptionOrValue< T >( std::forward< X >( value ) )
    {}

    NullOrValue( NullType ) :
        OptionOrValue< T >()
    {}

    bool null() const {
        return this->option();
    }

};

template< typename T >
struct PreventOrChange : OptionOrValue< T > {

    template< typename X >
    PreventOrChange( X &&value ) :
        OptionOrValue< T >( std::forward< X >( value ) )
    {}

    PreventOrChange( PreventValue ) :
        OptionOrValue< T >()
    {}

    bool preventFromChange() const {
        return this->option();
    }

};

template< typename T >
struct IsNull {
    static bool eval( const T & ) {
        return false;
    }
};
template< typename T >
struct IsNull< NullOrValue< T > > {
    static bool eval( const NullOrValue< T > &value ) {
        return value.null();
    }
};

template< typename T >
struct Alias : common::Comparable, common::Orderable {

    Alias( const T &ref ) :
        xRef( &ref )
    {}

    Alias( const OptionOrValue< T > &value ) :
        xRef( &value.value() )
    {}

    Alias( const T *ref = nullptr ) :
        xRef( ref )
    {}

    Alias &operator=( const Alias & ) = default;

    bool operator==( const T & other ) const {
        return *xRef == other;
    }
    bool operator<( const T &other ) const {
        return *xRef < other;
    }

    operator const T &( ) const {
        return *xRef;
    }

    bool valid() const {
        return xRef != nullptr;
    }

private:
    const T *xRef;
};


template< typename Self, typename Condition, typename Base, typename Row >
struct ConditionedIteratorBase : common::Comparable {
    static_assert(
        std::is_same< bool, typename std::result_of< Condition( const Row & ) >::type >::value,
        "the condition has to take row and return boolean" );

    ConditionedIteratorBase() = default;

    ConditionedIteratorBase( Condition condition, Base base, Base last ) :
        xCondition( condition ),
        xBase( base ),
        xLast( last )
    {}
    ConditionedIteratorBase( const ConditionedIteratorBase & ) = default;
    ConditionedIteratorBase &operator=( const ConditionedIteratorBase & ) = default;

    Self &operator++() {
        next();
        return self();
    }
    Self operator++( int ) {
        Self myself( *this );
        next();
        return myself;
    }

    bool operator==( const ConditionedIteratorBase &other ) const {
        return xBase == other.xBase;
    }

protected:

    Self &self() {
        return *static_cast< Self * >( this );
    }

    void first() {
        while ( xBase != xLast && !xCondition( *xBase ) )
            ++xBase;
    }

    void next() {
        ++xBase;
        first();
    }
    Condition xCondition;
    Base xBase;
    Base xLast;
};

template< typename Condition, bool Const, typename Base, typename Row, typename Column >
struct ConditionedIterator :
    ConditionedIteratorBase< ConditionedIterator< Condition, Const, Base, Row, Column >, Condition, Base, Row >,
    std::iterator< std::forward_iterator_tag, typename std::remove_reference< decltype( std::declval< Base >()->template get< Column >() ) >::type >
{
    using Parent = ConditionedIteratorBase< ConditionedIterator< Condition, Const, Base, Row, Column >, Condition, Base, Row >;
    using Parent::Parent;

    using ColumnType = decltype( std::declval< Base >()->template get< Column >() );

    auto operator*()
        -> typename std::conditional<
             Const,
             const typename std::remove_const< ColumnType >::type &,
             typename std::remove_const< ColumnType >::type &
        >::type
    {
        this->first();
        return this->xBase->template get< Column >();
    }

    auto operator->()
        -> typename std::conditional<
            Const,
            const typename std::remove_reference< ColumnType >::type *,
            typename std::remove_reference< ColumnType >::type *
        >::type
    {
        this->first();
        return &this->xBase->template get< Column >();
    }
};

template< typename Condition, bool Const, typename Base, typename Row >
struct ConditionedIterator< Condition, Const, Base, Row, void > :
    ConditionedIteratorBase< ConditionedIterator< Condition, Const, Base, Row, void >, Condition, Base, Row >,
    std::iterator< std::forward_iterator_tag, Row >
{
    using Parent = ConditionedIteratorBase< ConditionedIterator< Condition, Const, Base, Row, void >, Condition, Base, Row >;
    using Parent::Parent;

    auto operator*()
        -> typename std::conditional<
            Const,
            const Row &,
            Row &
        >::type
    {
        this->first();
        return *this->xBase;
    }

    auto operator->()
        -> typename std::conditional<
             Const,
             const Row *,
             Row *
        >::type
    {
        this->first();
        return this->xBase.operator->();
    }
};

struct LogicalAnd {
    void operator()( bool v ) {
        xStorage = xStorage && v;
    }

    bool value() const {
        return xStorage;
    }
private:
    bool xStorage = true;
};
struct LogicalOr {
    void operator()( bool v ) {
        xStorage = xStorage || v;
    }

    bool value() const {
        return xStorage;
    }
private:
    bool xStorage = false;
};
struct Void {};

struct Plain {
    void operator()( Void ) {
    }
    Void value() const {
        return Void();
    }
};

struct VoidFunctor {
    using Result = Void;
    using Combine = Plain;
    static Result defaultValue() {
        return Void();
    }
};
template< bool Value, typename Combinator >
struct BooleanFunctor {
    using Result = bool;
    using Combine = Combinator;
    static Result defaultValue() {
        return Value;
    }
};

struct FindIndexes : BooleanFunctor< false, LogicalOr > {
    template< typename Table, typename Key, typename ConstIterator >
    bool operator()( Table &table, const Key &key, ConstIterator ) {
        if ( !IsNull< Key >::eval( key ) )
            return table.find( key ) != table.end();
        return false;
    }
};
struct FindRelevantIndexes : BooleanFunctor< false, LogicalOr > {
    template< typename Table, typename Key, typename X1 >
    bool operator()( Table &table, const Key &key, X1 ) {
        if ( !key.preventFromChange() )
            return FindIndexes()( table, key.value(), nullptr );
        return false;
    }
};
struct InsertIndexes : BooleanFunctor< true, LogicalAnd > {
    template< typename Table, typename Key, typename Iterator >
    bool operator()( Table &table, const Key &key, Iterator i ) {
        if ( !IsNull< Key >::eval( key ) ) {
            auto result = table.emplace( key, i );
            if ( !result.second )
                return i == result.first->second;
        }
        return true;
    }
};
struct UpdateIndexes : VoidFunctor {
    template< typename Table, typename Key, typename Iterator >
    Void operator()( Table &table, const Key &key, Iterator i ) {
        if ( !IsNull< Key >::eval( key ) )
            table[ key ] = i;
        return Void();
    }
};
struct EraseIndexes : BooleanFunctor< true, LogicalAnd > {
    template< typename Table, typename Key, typename ConstIterator >
    bool operator()( Table &table, const Key &key, ConstIterator ) {
        if ( !IsNull< Key >::eval( key ) )
            return table.erase( key ) == 1;
        return true;
    }
};

struct ClearIndexes : VoidFunctor {
    template< typename Table, typename Key, typename ConstIterator >
    Void operator()( Table &table, const Key &, ConstIterator ) {
        table.clear();
        return Void();
    }
};

struct ReplaceValues : VoidFunctor {
    template< typename Table, typename OldValue, typename NewValue, typename X1, typename X2 >
    Void operator()( Table &, OldValue &oldValue, NewValue &newValue, X1, X2 ) {
        using std::swap;
        if ( !newValue.preventFromChange() ) {
            swap( oldValue, newValue.value() );
        }
        return Void();
    }
};

template< bool Const, typename Value, typename Base >
struct Iterator :
    std::iterator< std::bidirectional_iterator_tag, Value >,
    common::Comparable,
    common::Orderable
{
    template< bool _C, typename _V, typename _B >
    friend struct Iterator;

    Iterator() :
        _base( nullptr ),
        _offset( 0 )
    {}

    Iterator( Base *base, size_t offset = 0 ) :
        _base( base ),
        _offset( offset )
    {}

    Iterator( const Iterator & ) = default;

    template< typename B >
    Iterator( const Iterator< false, Value, B > &other ) :
        _base( other._base ),
        _offset( other._offset )
    {}
    Iterator &operator=( const Iterator & ) = default;

    auto operator*()
        -> typename std::conditional< Const, const Value &, Value & >::type
    {
        return *( base()->begin() + _offset );
    }
    const Value &operator*() const {
        return *( base()->begin() + _offset );
    }

    auto operator->()
        -> typename std::conditional< Const, const Value *, Value * >::type
    {
        return ( base()->begin() + _offset ).operator->();
    }
    const Value *operator->() const {
        return ( base()->begin() + _offset ).operator->();
    }

    Iterator &operator++() {
        ++_offset;
        return *this;
    }
    Iterator operator++( int ) {
        Iterator self( *this );
        ++_offset;
        return self;
    }
    Iterator &operator--() {
        --_offset;
        return *this;
    }
    Iterator operator--( int ) {
        Iterator self( *this );
        --_offset;
        return self;
    }

    Value &operator[]( size_t n ) {
        return *( *this + n );
    }
    const Value &operator[]( size_t n ) const {
        return *( *this + n );
    }

    std::ptrdiff_t operator-( const Iterator &other ) const {
        return ( base()->begin() + _offset ) - ( other.base()->begin() + other._offset );
    }
    Iterator &operator+=( size_t n ) {
        _offset += n;
        return *this;
    }
    Iterator &operator-=( size_t n ) {
        _offset -= n;
        return *this;
    }
    Iterator operator+( size_t n ) const {
        return Iterator( *this ) += n;
    }
    Iterator operator-( size_t n ) const {
        return Iterator( *this ) -= n;
    }

    bool operator==( const Iterator &other ) const {
        return ( base()->begin() + _offset ) == ( other.base()->begin() + other._offset );
    }

    bool operator<( const Iterator &other ) const {
        return ( base()->begin() + _offset ) < ( other.base()->begin() + other._offset );
    }

    operator typename std::conditional<
        Const,
        typename Base::const_iterator,
        typename Base::iterator
    >::type () const {
        return base()->begin() + _offset;
    }

private:
    const Base *base() const {
        return _base;
    }
    Base *base() {
        return _base;
    }
    Base *_base;
    size_t _offset;
};


template< bool Const, typename Condition, class Table, typename Column = void >
struct ConditionLayer {
    using Row = typename Table::Row;
    using Iterator = ConditionedIterator< Condition, Const, typename Table::Iterator, Row, Column >;
    using ConstIterator = ConditionedIterator< Condition, true, typename Table::ConstIterator, Row, Column >;

    using TablePtr = typename std::conditional<
        Const, const Table *, Table *
    >::type;

    using TableRef = typename std::conditional<
        Const, const Table &, Table &
    >::type;

    //using Value = typename std::conditional<
    //    std::is_same< Column, void >::value,
    //    typename Row::template Column< Column >::Type,
    //    void >::type;

    static_assert(
        std::is_same< bool, typename std::result_of< Condition( const Row & ) >::type >::value,
        "condition has to return boolean" );

    ConditionLayer( Condition c, TablePtr source ) :
        xCondition( c ),
        xSource( *source )
    {}

    ConditionLayer( ConditionLayer &&other ) :
        xCondition( std::move( other.xCondition ) ),
        xSource( other.xSource )
    {}

    template< bool C = Const >
    auto begin()
        -> typename std::enable_if< !C, Iterator >::type
    {
        return Iterator( xCondition, xSource.begin(), xSource.end() );
    }
    ConstIterator begin() const {
        return ConstIterator( xCondition, xSource.begin(), xSource.end() );
    }
    ConstIterator cbegin() const {
        return ConstIterator( xCondition, xSource.begin(), xSource.end() );
    }

    template< bool C = Const >
    auto end()
        -> typename std::enable_if< !C, Iterator >::type
    {
        return Iterator( xCondition, xSource.end(), xSource.end() );
    }
    ConstIterator end() const {
        return ConstIterator( xCondition, xSource.end(), xSource.end() );
    }
    ConstIterator cend() const {
        return ConstIterator( xCondition, xSource.end(), xSource.end() );
    }

    template< typename C >
    auto where( C condition )
        -> typename std::enable_if< !Const,
            ConditionLayer< false, C, ConditionLayer, Column >
        >::type
    {
        return ConditionLayer< false, C, ConditionLayer, Column >( condition, this );
    }

    template< typename C >
    ConditionLayer< true, C, ConditionLayer, Column > where( C condition ) const {
        return ConditionLayer< true, C, ConditionLayer, Column >( condition, this );
    }

    template< typename Selected >
    auto select()
        -> typename std::enable_if<
          !Const && std::is_same< void, Column >::value,
          ConditionLayer< false, Condition, Table, Selected >
        >::type
    {
        return ConditionLayer< false, Condition, Table, Selected >( xCondition, &xSource );
    }

    template< typename Selected >
    auto select() const
        -> typename std::enable_if<
        std::is_same< void, Column >::value,
        ConditionLayer< true, Condition, Table, Selected >
        >::type
    {
        return ConditionLayer< true, Condition, Table, Selected >( xCondition, &xSource );
    }

private:

    Condition xCondition;
    TableRef xSource;
};

struct AllToTrue {
    template< typename T >
    bool operator()( const T & ) const {
        return true;
    }
};

// implementation of std::shared_mutex from C++17
struct SharedMutex {
    SharedMutex() :
        _readers( 0 )
    {}
    SharedMutex &operator=( const SharedMutex & ) = delete;

    void lock() {
        _reader.lock();
        _writer.lock();
    }
    bool try_lock() {
        if ( !_reader.try_lock() )
            return false;
        if ( !_writer.try_lock() ) {
            _reader.unlock();
            return false;
        }
        return true;
    }
    void promote() {
        _reader.lock();
        if ( --_readers )
            _writer.lock();
    }
    void unlock() {
        _writer.unlock();
        _reader.unlock();
    }

    void lock_shared() {
        std::lock_guard< std::mutex > rLock( _reader );
        if ( ++_readers == 1 )
            _writer.lock();
    }
    bool try_lock_shared() {
        std::unique_lock< std::mutex > rLock( _reader, std::try_to_lock );
        if ( !rLock.owns_lock() )
            return false;
        if ( ++_readers == 1 ) {
            if ( !_writer.try_lock() ) {
                --_readers;
                return false;
            }
        }
        return true;
    }
    void unlock_shared() {
        if ( --_readers == 0 )
            _writer.unlock();
    }
private:
    struct SignaledMutex {

        SignaledMutex() :
            _open( true )
        {}

        void lock() {
            std::unique_lock< std::mutex > lock( _mutex );
            _signal.wait( lock, [this] { return _open; } );
            _open = false;
        }
        bool try_lock() {
            std::unique_lock< std::mutex > lock( _mutex );
            if ( !_open )
                return false;
            _open = false;
            return true;
        }
        void unlock() {
            std::unique_lock< std::mutex > lock( _mutex );
            _open = true;
            lock.unlock();
            _signal.notify_one();
        }

    private:
        std::mutex _mutex;
        std::condition_variable _signal;
        bool _open;
    };

    std::mutex _reader;
    SignaledMutex _writer;
    std::atomic< int > _readers;
};


} // namespace detail

namespace list {

template< bool Expr, bool... Rest >
struct BooleanList {
    static constexpr bool head = Expr;
    using Tail = BooleanList< Rest... >;

    static constexpr bool op_and = Expr && Tail::op_and;
    static constexpr bool op_or = Expr || Tail::op_or;
};

template< bool Expr >
struct BooleanList< Expr > {
    static constexpr bool head = Expr;
    static constexpr bool op_and = Expr;
    static constexpr bool op_or = Expr;
};

template< size_t, typename... >
struct Find {};

template< size_t N, typename Needle, typename Head, typename... Tail >
struct Find< N, Needle, Head, Tail... > : Find< N + 1, Needle, Tail... > {};

template< size_t _N, typename Needle, typename... Tail >
struct Find< _N, Needle, Needle, Tail... > {
    static constexpr size_t N = _N;
    static constexpr bool found = true;
};

template< size_t _N, typename Needle >
struct Find< _N, Needle > {
    static constexpr size_t N = _N;
    static constexpr bool found = false;
};

template< size_t N, typename Head, typename... Tail >
struct Get : Get< N - 1, Tail... > {};

template< typename Head, typename... Tail >
struct Get< 0, Head, Tail... > {
    using Type = Head;
};

template< size_t start, size_t stop, typename... List >
struct AllUnique : AllUnique< start + 1, stop, List... > {

    using Finder = Find< 0, typename Get< start, List... >::Type, List... >;

    static constexpr bool result =
        Finder::found &&
        start == Finder::N &&
        AllUnique< start + 1, stop, List... >::result;
};

template< size_t stop, typename... List >
struct AllUnique< stop, stop, List... > {
    static constexpr bool result = true;
};

template< typename... Empty >
struct InheritAll {};

template< typename Head, typename... Tail >
struct InheritAll< Head, Tail... > : Head::template T< InheritAll< Tail... > > {
};

} // namespace list



template< typename Value, typename Allocator >
struct StorageList : std::list< Value, Allocator > {
    using Base = std::list< Value, Allocator >;
    using Iterator = typename Base::iterator;
    using ConstIterator = typename Base::const_iterator;

    using Base::Base;

    template< typename C >
    Iterator erase( ConstIterator position, C ) {
        return Base::erase( position );
    }
};

template< typename Value, typename Allocator >
struct StorageVector : std::vector< Value, Allocator > {
    using Base = std::vector< Value, Allocator >;

    using Iterator = detail::Iterator< false, Value, Base >;
    using ConstIterator = detail::Iterator< true, Value, const Base >;

    using Base::Base;

    Iterator begin() {
        return Iterator( this, 0 );
    }
    ConstIterator begin() const {
        return ConstIterator( this, 0 );
    }
    ConstIterator cbegin() const {
        return ConstIterator( this, 0 );
    }
    Iterator end() {
        return Iterator( this, this->size() );
    }
    ConstIterator end() const {
        return ConstIterator( this, this->size() );
    }
    ConstIterator cend() const {
        return ConstIterator( this, this->size() );
    }

    template< typename C >
    Iterator erase( ConstIterator position, C c ) {
        auto it = Base::erase( static_cast< typename Base::const_iterator >( position ) );
        Iterator i( this, it - Base::begin() );

        for ( ; i != end(); ++i ) {
            c( i );
        }
        return i;
    }
};

template< typename Self, typename Table, typename... Columns >
struct BaseConnector : common::Comparable {

    using Iterator = typename Table::Iterator;
    using ConstIterator = typename Table::ConstIterator;
    using Row = typename Table::Row;
    using AccessKey = typename Table::AccessKey;

    // STL type interface
    using value_type = typename Table::value_type;
    using reference = typename Table::reference;
    using const_reference = typename Table::const_reference;
    using difference_type = std::ptrdiff_t;
    using size_type = std::size_t;
    using iterator = typename Table::iterator;
    using const_iterator = typename Table::const_iterator;
    using allocator = typename Table::allocator;

    BaseConnector( Table &table, AccessKey key ) :
        xTable( table ),
        xAccessKey( key )
    {}
    BaseConnector( const BaseConnector & ) = default;

    bool operator==( const BaseConnector &other ) const {
        return table().equals( xAccessKey, other );
    }

    void clear() {
        table().clear( xAccessKey );
    }

    size_t size() const {
        return table().size( xAccessKey );
    }
    bool empty() const {
        return table().empty( xAccessKey );
    }

    std::pair< ConstIterator, bool > insert( typename Columns::Type... args ) {
        return table().insert( xAccessKey, Row( std::move( args )... ) );
    }
    std::pair< ConstIterator, bool > insert( Row item ) {
        return table().insert( xAccessKey, std::move( item ) );
    }

    template< typename Name >
    Iterator find( const typename Table::template GetColumnByName< Name >::PureType &key ) {
        return table().template find< Table::template GetColumnIndex< Name >::index >( xAccessKey, key );
    }

    template< typename Name >
    ConstIterator find( const typename Table::template GetColumnByName< Name >::PureType &key ) const {
        return table().template find< Table::template GetColumnIndex< Name >::index >( xAccessKey, key );
    }

    template< typename Name >
    bool update( const typename Table::template GetColumnByName< Name >::PureType &key, typename Columns::ChangeType... args ) {
        return table().template update< Table::template GetColumnIndex< Name >::index >( xAccessKey, key, std::move( args )... );
    }
    template< typename Name >
    bool update( const typename Table::template GetColumnByName< Name >::PureType &key, Row row ) {
        return table().template update< Table::template GetColumnIndex< Name >::index >( xAccessKey, key, std::move( row ) );
    }

    template< typename Name >
    ConstIterator erase( const typename Table::template GetColumnByName< Name >::PureType &key ) {
        return table().template erase< Table::template GetColumnIndex< Name >::index >( xAccessKey, key );
    }

    template< typename Condition >
    size_t eraseIf( Condition condition ) {
        Iterator i = begin();
        size_t count = 0;
        while ( i != end() ) {
            if ( condition( *i ) ) {
                i = table().erase( xAccessKey, i );
                ++count;
            }
            else
                ++i;
        }
        return count;
    }

    template< typename Condition >
    detail::ConditionLayer< false, Condition, Self > where( Condition condition ) {
        static_assert(
            std::is_same< bool, typename std::result_of< Condition( const Row & ) >::type >::value,
            "condition has to return boolean" );

        return detail::ConditionLayer< false, Condition, Self >( condition, &self() );
    }
    template< typename Condition >
    detail::ConditionLayer< true, Condition, Self > where( Condition condition ) const {
        static_assert(
            std::is_same< bool, typename std::result_of< Condition( const Row & ) >::type >::value,
            "condition has to return boolean" );

        return detail::ConditionLayer< true, Condition, Self >( condition, &self() );
    }

    template< typename Column >
    detail::ConditionLayer< false, detail::AllToTrue, Self, Column > select() {
        static_assert(
            list::Find< 0, Column, typename Columns::Name... >::found,
            "name has to be listed inside table" );

        return detail::ConditionLayer< false, detail::AllToTrue, Self, Column >( detail::AllToTrue{}, &self() );
    }
    template< typename Column >
    detail::ConditionLayer< true, detail::AllToTrue, Self, Column > select() const {
        static_assert(
            list::Find< 0, Column, typename Columns::Name... >::found,
            "name has to be listed inside table" );

        return detail::ConditionLayer< true, detail::AllToTrue, Self, Column >( detail::AllToTrue{}, &self() );
    }

    Iterator begin() {
        return table().begin( xAccessKey );
    }
    ConstIterator begin() const {
        return table().begin( xAccessKey );
    }
    ConstIterator cbegin() const {
        return table().cbegin( xAccessKey );
    }

    Iterator end() {
        return table().end( xAccessKey );
    }
    ConstIterator end() const {
        return table().end( xAccessKey );
    }
    ConstIterator cend() const {
        return table().cend( xAccessKey );
    }
protected:
    Self &self() {
        return *static_cast< Self * >( this );
    }
    const Self &self() const {
        return *static_cast< const Self * >( this );
    }

    Table &table() {
        return xTable;
    }
    const Table &table() const {
        return xTable;
    }
private:
    Table &xTable;
    AccessKey xAccessKey;
};

template< typename Table, typename... Columns >
struct LocalConnector : BaseConnector< LocalConnector< Table, Columns... >, Table, Columns... > {
    struct Shared {};

    LocalConnector( Table &table ) :
        BaseConnector< LocalConnector< Table, Columns... >, Table, Columns... >( table, {} )
    {}
};

template< typename Table, typename... Columns >
struct SharedConnector : BaseConnector< SharedConnector< Table, Columns... >, Table, Columns... > {

    using Shared = detail::SharedMutex;
    using Base = BaseConnector< SharedConnector< Table, Columns... >, Table, Columns... >;
    using Self = SharedConnector;
    using ConstIterator = typename Base::ConstIterator;
    using Iterator = typename Base::Iterator;
    using Row = typename Base::Row;

    SharedConnector( Table &table ) :
        Base( table, {} ),
        xState( State::Reader )
    {
        this->table().shared( {} ).lock_shared();
    }

    SharedConnector( const SharedConnector & ) = delete;
    SharedConnector( SharedConnector &&other ) :
        Base( std::move( other ) ),
        xState( other.xState )
    {
        other.xState = State::Free;
    }

    ~SharedConnector() {
        switch ( xState ) {
        case State::Free:
            break;
        case State::Reader:
            this->table().shared( {} ).unlock_shared();
            break;
        case State::Writer:
            this->table().shared( {} ).unlock();
            break;
        }
        // be idempotent
        xState = State::Free;
    }

    void requireLocked() {
        if ( xState == State::Free )
            throw std::runtime_error( "disconnected connector" );
    }

    void promote() {
        requireLocked();
        if ( xState == State::Reader ) {
            this->table().shared( {} ).promote();
            xState = State::Writer;
        }
    }

    void reset() {
        if ( xState == State::Writer ) {
            this->table().shared( {} ).unlock();
            this->table().shared( {} ).lock_shared();
            xState = State::Reader;
        }
    }

    const SharedConnector &readOnly() const {
        return *this;
    }

    void clear() {
        promote();
        Base::clear();
        reset();
    }

    std::pair< ConstIterator, bool > insert( typename Columns::Type... args ) {
        promote();
        return Base::insert( std::forward< typename Columns::Type >( args )... );
    }
    std::pair< ConstIterator, bool > insert( Row row ) {
        promote();
        return Base::insert( std::move( row ) );
    }

    template< typename Name >
    ConstIterator find( const typename Table::template GetColumnByName< Name >::PureType &key ) const {
        return Base::template find< Name >( key );
    }

    template< typename Name >
    bool update( const typename Table::template GetColumnByName< Name >::PureType &key, typename Columns::ChangeType... args ) {
        promote();
        return Base::template update< Name >( key, std::move( args )... );
    }
    template< typename Name >
    bool update( const typename Table::template GetColumnByName< Name >::PureType &key, Row row ) {
        promote();
        return Base::template update< Name >( key, std::move( row ) );
    }

    template< typename Name >
    ConstIterator erase( const typename Table::template GetColumnByName< Name >::PureType &key ) {
        promote();
        return Base::template erase< Name >( key );
    }

    template< typename Condition >
    size_t eraseIf( Condition condition ) {
        promote();
        typename Base::Iterator i = Base::begin();
        size_t count = 0;
        while ( i != Base::end() ) {
            if ( condition( *i ) ) {
                i = this->table().erase( {}, i );
                ++count;
            }
            else
                ++i;
        }
        return count;
    }

    template< typename Condition >
    detail::ConditionLayer< false, Condition, Self > where( Condition condition ) {
        static_assert(
            std::is_same< bool, typename std::result_of< Condition( const Row & ) >::type >::value,
            "condition has to return boolean" );

        promote();
        return detail::ConditionLayer< false, Condition, Self >( condition, this );
    }

    template< typename Condition >
    detail::ConditionLayer< true, Condition, Self > where( Condition condition ) const {
        static_assert(
            std::is_same< bool, typename std::result_of< Condition( const Row & ) >::type >::value,
            "condition has to return boolean" );

        return detail::ConditionLayer< true, Condition, Self >( condition, this );
    }
    template< typename Condition >
    detail::ConditionLayer< true, Condition, Self > constWhere( Condition condition ) const {
        static_assert(
            std::is_same< bool, typename std::result_of< Condition( const Row & ) >::type >::value,
            "condition has to return boolean" );

        return detail::ConditionLayer< true, Condition, Self >( condition, this );
    }

    template< typename Column >
    detail::ConditionLayer< false, detail::AllToTrue, Self, Column > select() {
        static_assert(
            list::Find< 0, Column, typename Columns::Name... >::found,
            "name has to be listed inside table" );

        promote();
        return detail::ConditionLayer< false, detail::AllToTrue, Self, Column >( detail::AllToTrue{}, this );
    }
    template< typename Column >
    detail::ConditionLayer< true, detail::AllToTrue, Self, Column > select() const {
        static_assert(
            list::Find< 0, Column, typename Columns::Name... >::found,
            "name has to be listed inside table" );

        return detail::ConditionLayer< true, detail::AllToTrue, Self, Column >( detail::AllToTrue{}, this );
    }
    template< typename Column >
    detail::ConditionLayer< true, detail::AllToTrue, Self, Column > constSelect() const {
        static_assert(
            list::Find< 0, Column, typename Columns::Name... >::found,
            "name has to be listed inside table" );

        return detail::ConditionLayer< true, detail::AllToTrue, Self, Column >( detail::AllToTrue{}, this );
    }

    Iterator begin() {
        promote();
        return Base::begin();
    }
    ConstIterator begin() const {
        return Base::begin();
    }
    ConstIterator cbegin() const {
        return Base::cbegin();
    }

    Iterator end() {
        promote();
        return Base::end();
    }
    ConstIterator end() const {
        return Base::end();
    }
    ConstIterator cend() const {
        return Base::cend();
    }

private:
    enum class State {
        Free,
        Reader,
        Writer
    };
    State xState;
};

template<
    template< typename, typename... > class _Connector,
    template< typename, typename > class _Underlying = StorageList,
    template< typename > class _Allocator = std::allocator
>
struct Database {

    template< typename Value, typename Allocator >
    using Underlying = _Underlying< Value, Allocator >;
    template< typename T >
    using Allocator = _Allocator< T >;

    template< typename Key, typename Value >
    using IndexTree = std::map< Key, Value, std::less< Key >, Allocator< std::pair< Key, Value > > >;

    template< typename Key, typename Value >
    using IndexHash = std::unordered_map< Key, Value, std::hash< Key >, std::equal_to< Key >, Allocator< std::pair< const Key, Value > > >;

    template< template< typename, typename > class IndexStorage = IndexTree >
    struct Index {

        template< typename Base >
        struct T : Base {
            static constexpr bool indexed = true;

            template< typename Key, typename Value >
            using IndexTable = IndexStorage< Key, Value >;
        };
    };

    struct NotNull {

        template< typename Base >
        struct T : Base {
            static constexpr bool null = false;
        };
    };

    static constexpr NullType Null = NullType::Null;
    static constexpr PreventValue Unchanged = PreventValue::Unchanged;

private:
    struct IgnoreIndex {};

    struct DefaultOption {
        template< typename Base >
        struct T {
            static constexpr bool indexed = false;
            static constexpr bool null = true;

            template< typename Key, typename Value >
            using IndexTable = IgnoreIndex;
        };
    };

public:
    template< typename _Name, typename _Type, typename... Options >
    struct Column : list::InheritAll< Options..., DefaultOption > {
        using Name = _Name;
        using PureType = _Type;
        using Type = typename std::conditional<
            list::InheritAll< Options..., DefaultOption >::null,
            detail::NullOrValue< _Type >,
            _Type >::type;
        using ChangeType = detail::PreventOrChange< Type >;
    };

    template< typename... Columns >
    struct Table : common::Comparable {

        static_assert(
            sizeof...( Columns ) > 0,
            "at least one column has to be specified" );

        static_assert(
            list::AllUnique< 0, sizeof...( Columns ), typename Columns::Name... >::result,
            "column names has to be unique" );

        static_assert(
            list::BooleanList< Columns::indexed... >::op_or,
            "at least one field has to be indexed" );

        template< size_t N >
        struct GetColumn : list::Get< N, Columns... >::Type {
            static_assert(
                N < sizeof...( Columns ),
                "column index is out of range" );
        };

        template< typename Name >
        struct GetColumnIndex {

            using Result = list::Find< 0, Name, typename Columns::Name... >;

            static_assert(
                Result::found,
                "requested name was not found" );

            static constexpr const size_t index = Result::N;
        };

        template< typename Name >
        struct GetColumnByName : GetColumn< GetColumnIndex< Name >::index > {
        };

        struct Row : std::tuple< typename Columns::Type... > {
            using Tuple = std::tuple< typename Columns::Type... >;
            using Tuple::Tuple;

            template< typename Name >
            using Column = GetColumnByName< Name >;

            template< typename Name, size_t N = GetColumnIndex< Name >::index >
            auto get()
                -> typename std::enable_if<
                  !GetColumn< N >::indexed,
                  typename GetColumn< N >::Type &
                >::type
            {
                return std::get< N >( *static_cast< Tuple * >( this ) );
            }

            template< typename Name, size_t N = GetColumnIndex< Name >::index >
            auto get() const
                -> const typename GetColumn< N >::Type &
            {
                return std::get< N >( *static_cast< const Tuple * >( this ) );
            }

            void swap( Row &other ) {
                Tuple::swap( other );
            }
        };

        using Connector = _Connector< Table, Columns... >;

        class AccessKey {
            friend Connector;
            friend Table;
            AccessKey() {}
        public:
            AccessKey( const AccessKey & ) = default;
        };

        using Storage = Underlying< Row, Allocator< Row > >;
        using Iterator = typename Storage::Iterator;
        using ConstIterator = typename Storage::ConstIterator;
        static constexpr const size_t columns = sizeof...( Columns );

        // STL type interface
        using value_type = Row;
        using reference = Row &;
        using const_reference = const Row &;
        using difference_type = std::ptrdiff_t;
        using size_type = std::size_t;
        using iterator = Iterator;
        using const_iterator = ConstIterator;
        using allocator = Allocator< Row >;

        Table( std::initializer_list< Row > items )
        {
            for ( const Row &r : items )
                insert( {}, r );
        }

        template< typename It >
        Table( It first, It last )
        {
            for ( ; first != last; ++last )
                insert( {}, *first );
        }

        Table() = default;
        Table( const Table & ) = delete;
        Table( Table && ) = delete;
        ~Table() = default;

        Connector connect() {
            return Connector( *this );
        }

    //access key required:
        bool equals( AccessKey, const Table &other ) const {
            return xData = other._data;
        }

        size_t size( AccessKey ) const {
            return xData.size();
        }
        bool empty( AccessKey ) const {
            return xData.empty();
        }

        void clear( AccessKey ) {
            if ( !xData.empty() ) {
                iterateIndexed( detail::ClearIndexes(), begin( {} ) );
                xData.clear();
            }
        }

        std::pair< Iterator, bool > insert( AccessKey, Row item ) {
            xData.emplace_back( std::move( item ) );
            auto it = --xData.end();
            if ( iterateIndexed( detail::FindIndexes(), it ) ) {
                xData.pop_back();
                return{ end( {} ), false };
            }

            iterateIndexed( detail::InsertIndexes(), it );
            return{ --end( {} ), true };
        }

        template< size_t N >
        ConstIterator find( AccessKey, const typename GetColumn< N >::Type &key ) const {
            static_assert(
                GetColumn< N >::indexed,
                "column is not indexed" );

            const auto &table = std::get< N >( xIndexesTables );
            auto i = table.find( key );
            if ( i == table.end() )
                return end( {} );
            return i->second;
        }

        template< size_t N >
        Iterator find( AccessKey, const typename GetColumn< N >::Type &key ) {
            static_assert(
                GetColumn< N >::indexed,
                "column is not indexed" );

            const auto &table = std::get< N >( xIndexesTables );
            auto i = table.find( key );
            if ( i == table.end() )
                return end( {} );
            return i->second;
        }

        template< size_t N >
        bool update( AccessKey, const typename GetColumn< N >::Type &key, Row row ) {
            static_assert(
                GetColumn< N >::indexed,
                "column is not indexed" );

            auto &table = std::get< N >( xIndexesTables );
            auto i = table.find( key );
            if ( i == table.end() )
                return false;

            bool result = true;
            Iterator persistent = i->second;
            iterateIndexed( detail::EraseIndexes(), persistent );
            if ( iterateIndexed( detail::FindIndexes(), &row ) )
                result = false;
            else
                row.swap( *persistent );
            iterateIndexed( detail::InsertIndexes(), persistent );
            return result;
        }

        template< size_t N >
        bool update( AccessKey, const typename GetColumn< N >::Type &key, typename Columns::ChangeType &&... values ) {
            static_assert(
                GetColumn< N >::indexed,
                "column is not indexed" );

            auto newRow = std::make_tuple( std::forward< typename Columns::ChangeType >( values )... );

            auto &table = std::get< N >( xIndexesTables );
            auto i = table.find( key );
            if ( i == table.end() )
                return false;

            bool result = false;
            Iterator persistent = i->second;

            iterateIndexed( detail::EraseIndexes(), persistent );

            if ( iterateIndexed( detail::FindRelevantIndexes(), &newRow ) )
                result = false;
            else
                iterateAll( detail::ReplaceValues(), persistent, &newRow );
            iterateIndexed( detail::InsertIndexes(), persistent );
            return result;
        }

        Iterator erase( AccessKey, ConstIterator row ) {
            if ( row == end( {} ) )
                return end( {} );

            iterateIndexed( detail::EraseIndexes(), row );
            return xData.erase( row, [this]( Iterator i ) {
                this->iterateIndexed( detail::UpdateIndexes(), i );
            } );
        }

        template< size_t N >
        Iterator erase( AccessKey, const typename GetColumn< N >::Type &key ) {
            static_assert(
                GetColumn< N >::indexed,
                "column is not indexed" );

            auto &table = std::get< N >( xIndexesTables );
            auto i = table.find( key );
            if ( i == table.end() )
                return end( {} );

            return erase( {}, i->second );
        }

        Iterator begin( AccessKey ) {
            return xData.begin();
        }
        ConstIterator begin( AccessKey ) const {
            return xData.begin();
        }
        ConstIterator cbegin( AccessKey ) const {
            return xData.begin();
        }

        Iterator end( AccessKey ) {
            return xData.end();
        }
        ConstIterator end( AccessKey ) const {
            return xData.end();
        }
        ConstIterator cend( AccessKey ) const {
            return xData.cend();
        }

        typename Connector::Shared &shared( AccessKey ) {
            return xShared;
        }

    private:

        template< bool E, typename F, typename... Args >
        static auto callIf( F f, Args &&... args )
            -> typename std::enable_if< E, typename F::Result >::type
        {
            return f( std::forward< Args >( args )... );
        }

        template< bool E, typename F, typename... Args >
        static auto callIf( F, Args &&... )
            -> typename std::enable_if< !E, typename F::Result >::type
        {
            return F::defaultValue();
        }

        template< bool B, size_t N, typename F, typename... It >
        auto iterate( F f, It... it )
            -> typename std::enable_if< N == 0, typename F::Result >::type
        {
            constexpr const bool realCall = B || GetColumn< N >::indexed;
            return callIf< realCall >(
                f,
                std::get< 0 >( xIndexesTables ),
                std::get< 0 >( *it )... ,
                it... );
        }

        template< bool B, size_t N, typename F, typename... It >
        auto iterate( F f, It... it )
            -> typename std::enable_if< ( N > 0 ), typename F::Result >::type
        {
            typename F::Combine combine;
            constexpr const bool realCall = B || GetColumn< N >::indexed;

            combine( iterate< B, ( N - 1 )  >( f, it... ) );
            combine( callIf< realCall >(
                    f,
                    std::get< N >( xIndexesTables ),
                    std::get< N >( *it )...,
                    it... ) );

            return combine.value();
        }


        template< typename F, typename... It >
        typename F::Result iterateIndexed( F f, It... it ) {
            return iterate< false, columns - 1 >( f, it... );
        }

        template< typename F, typename... It >
        typename F::Result iterateAll( F f, It... it ) {
            return iterate< true, columns - 1 >( f, it... );
        }

        Storage xData;
        std::tuple<
            typename Columns::template IndexTable< detail::Alias< typename Columns::PureType >, Iterator >...
        > xIndexesTables;
        typename Connector::Shared xShared;
    };
};

} // namespace db
} // namespace brick

namespace std {

template< typename T >
struct hash< ::brick::db::detail::Alias< T > > {
    size_t operator()( const ::brick::db::detail::Alias< T > &alias ) const {
        return hash< T >()( alias );
    }
};

} // namespace std

#if 0

namespace brick_test {
namespace db {

using namespace brick::db;

struct Local {

    TEST( elementary ) {

        struct ID {};
        struct Name {};

        using DB = Database< LocalConnector >;
        using Table = DB::Table<
            DB::Column< ID, int, DB::Index< DB::IndexHash >, DB::NotNull >,
            DB::Column< Name, const char *, DB::NotNull >
        >;

        Table table;
        auto Connector = table.Connector();

        Connector.insert( 1, "test" );
    }

};

} // namespace db
} // namespace brick_test

#endif

#endif
