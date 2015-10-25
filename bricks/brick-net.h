#include <memory>
#include <atomic>
#include <stdexcept>
#include <iterator>
#include <string>
#include <cstring>
#include <queue>
#include <mutex>
#include <thread>
#include <chrono>
#include <algorithm>
#include <type_traits>

#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#include <brick-common.h>
#include <brick-unittest.h>

#ifndef BRICK_NET_H
#define BRICK_NET_H

namespace brick {
namespace net {

namespace settings {
    // set default timeout to sockets
    static constexpr int timeout = 1000;
}

struct NetException : std::exception {};

struct SystemException : NetException {
    // make sure we don't override errno accidentaly when constructing std::string
    explicit SystemException( std::string what ) : SystemException{ errno, what } {}
    explicit SystemException( const char *what ) : SystemException{ errno, what } {}
    explicit SystemException( int _errno, std::string what ) {
        _what = "System error: " + std::string( std::strerror( _errno ) ) + ", when " + what;
    }

    const char *what() const noexcept override { return _what.c_str(); }

private:
    std::string _what;

};

struct ConnectionAbortedException : NetException {
    explicit ConnectionAbortedException( std::string what ) :
        _what{ "Connection error: peer has disconnected during " + what }
    {}
    explicit ConnectionAbortedException( const char *what ) :
        ConnectionAbortedException{ std::string{ what } }
    {}

    const char *what() const noexcept override { return _what.c_str(); }

private:
    std::string _what;
};

struct ConnectionTimeout : NetException {
    explicit ConnectionTimeout( std::string to ) {
        _what = "Timeout on connection to " + to;
    }
    const char *what() const noexcept override {
        return _what.c_str();
    }
private:
    std::string _what;
};

struct DataTransferException : NetException {
    DataTransferException( const char *what ) :
        _what( what )
    {}

    const char *what() const noexcept override {
        return _what.c_str();
    }
private:
    std::string _what;
};

struct WouldBlock : NetException {
    const char *what() const noexcept override { return "would block"; }
};

enum class Shutdown : int {
    Read = SHUT_RD,
    Write = SHUT_WR,
    Both = SHUT_RDWR
};

struct IOvector : iovec {

    IOvector() = default;
    IOvector( const IOvector & ) = default;
    IOvector( void *d, size_t s )
    {
        data( d );
        size( s );
    }

    template< typename T = char >
    const T *data() const {
        return reinterpret_cast< const T * >( iov_base );
    }
    template< typename T = char >
    T *data() {
        return reinterpret_cast< T * >( iov_base );
    }
    void data( void *d ) {
        iov_base = d;
    }

    size_t size() const {
        return iov_len;
    }
    void size( size_t s ) {
        iov_len = s;
    }
};

struct PollFD : pollfd {
    PollFD( int fd, int events ) {
        this->fd = fd;
        this->events = events;
        this->revents = 0;
    }
};

class Message {
    struct Header {

        Header() {
            clear();
        }

        void clear() {
            std::memset( this, 0, sizeof( Header ) );
        }

        static constexpr const size_t PROPERTIES = 2;
        static constexpr const size_t SEGMENTS = 255;
        static constexpr const size_t HEAD =
            sizeof( uint8_t ) + sizeof( uint8_t ) + // category + count
            sizeof( uint8_t ) + sizeof( uint8_t ) + // id + padding
            sizeof( int32_t );                      // tag
        static constexpr const size_t SIZE =
            HEAD + sizeof( uint32_t ) * SEGMENTS;

        size_t size() const {
            return HEAD + count * sizeof( uint32_t );
        }

        uint8_t category;
        uint8_t count;
        uint8_t from;
        uint8_t to;
        int32_t tag;
        uint32_t segments[ SEGMENTS ];
    };

    static_assert(
        Header::SIZE == sizeof( Header ),
        "wrong length of header of message" );
public:

    template< typename T = uint8_t >
    Message( T category = T() ) :
        _header{},
        _data{ { &_header, _header.size() } },
        _readIndex( 1 ),
        _allocated( false )
    {
        _header.category = static_cast< uint8_t >( category );
    }

    static Message dynamicMessage() {
        Message m;
        m._allocated = true;
        return m;
    }

    ~Message() {
        reset();
    }

    template< typename T = int >
    T category() const {
        return static_cast< T >( _header.category );
    }

    template< typename T = int >
    T tag() const {
        return static_cast< T >( _header.tag );
    }
    template< typename T >
    void tag( T t ) {
        _header.tag = static_cast< int32_t >( t );
    }

    int from() const {
        return _header.from;
    }
    void from( int f ) {
        _header.from = f;
    }
    int to() const {
        return _header.to;
    }
    void to( int t ) {
        _header.to = t;
    }

    bool full() const {
        return _header.count == Header::SEGMENTS || _allocated;
    }

    bool add( void *data, uint32_t length ) {
        if ( full() )
            return false;
        _header.segments[ _header.count ] = length;
        ++_header.count;
        _data.emplace_back( data, length );
        updatedHeaderSize();
        return true;
    }

    template< typename T >
    auto operator<<( T &data )
#ifndef __GLIBCXX__
        -> typename std::enable_if<
            std::is_trivially_copyable< T >::value,
            Message &
        >::type
#else
    -> Message &
#endif
    {
        add( &data, sizeof( T ) );
        return *this;
    }
    template< typename T >
    auto operator>>( T &data )
#ifndef __GLIBCXX__
        -> typename std::enable_if<
            std::is_trivially_copyable< T >::value,
            Message &
        >::type
#else
    -> Message &
#endif
    {
        auto *v = read();
        if ( v )
            data = *v->data< T >();
        return *this;
    }

    Message &operator<<( std::string &data ) {
        add( &data.front(), data.size() );
        return *this;
    }
    Message &operator>>( std::string &data ) {
        auto *v = read();
        if ( v )
            data.assign( v->data(), v->size() );
        return *this;
    }

    template< typename T >
    auto operator<<( std::vector< T > &data )
#ifndef __GLIBCXX__
        -> typename std::enable_if<
            std::is_trivially_copyable< T >::value,
            Message &
        >::type
#else
    -> Message &
#endif
    {
        add( data.data(), data.size() * sizeof( T ) );
        return *this;
    }
#if 0
    template< typename T >
    auto operator>>( std:vector< T > &data )
        -> typename std::enable_if<
            std::is_trivially_copyable< T >::value,
            Message &
        >::type
    {
        auto *v = read();
        if ( v )
            data.assign(  )
        return *this;
    }
#endif

    bool canRead() const {
        return _readIndex <= count();
    }
    void resetReading() {
        _readIndex = 1;
    }
    Message &storeTo() {
        resetReading();
        return *this;
    }
    IOvector *read() {
        if ( !canRead() )
            return nullptr;
        return &_data[ _readIndex++ ];
    }

    size_t count() const {
        return _header.count;
    }

    size_t length() const {
        size_t l = 0;
        for ( const auto &i : _data )
            l += i.size();
        return l;
    }

    void disown() {
        _allocated = false;
    }
    std::vector< IOvector >::iterator begin() {
        return _data.begin() + 1;
    }
    std::vector< IOvector >::iterator end() {
        return _data.end();
    }

    void reset() {
        if ( _allocated )
            // skip first item -- _header
            for ( auto &i : *this ) {
                delete[] i.data();
            }
        uint8_t cat = _header.category;
        _header.clear();
        _header.category = cat;
        _data.clear();
        _readIndex = 1;
        _allocated = false;
        _data.emplace_back( &_header, _header.size() );
    }

    Header &header() {
        return _header;
    }
    const Header &header() const {
        return _header;
    }

    void allocateData() {
        updatedHeaderSize();
        if ( _data.size() != 1 ) {
            if ( _data.size() < _header.count )
                throw DataTransferException( "incomming message is too long" );
            return;
        }
        _data.reserve( Header::SEGMENTS );
        for ( uint8_t i = 0; i != _header.count; ++i ) {
            size_t segmentSize = _header.segments[ i ];
            if ( segmentSize == 0 )
                break;
            _data.emplace_back( new char[ segmentSize ], segmentSize );
        }
    }

    IOvector *ioVector() {
        return _data.data(); // skip first item -- _header
    }

    size_t ioVectorSize() const {
        return _data.size();
    }
private:

    void updatedHeaderSize() {
        _data.front().size( _header.size() );
    }

    Header _header;
    std::vector< IOvector > _data;
    size_t _readIndex;
    bool _allocated;
};

enum class Access : bool {
    Read,
    Write
};

struct Socket : common::Comparable, common::Orderable {

    enum { Invalid = -1 };

    Socket() :
        _fd( Invalid ),
        _blocking( true )
    {}
    explicit Socket( int fd ) :
        _fd( fd ),
        _blocking( true )
    {}
    Socket( const Socket & ) = delete;
    Socket( Socket &&other ) :
        _fd( Invalid )
    {
        swap( other );
    }
    ~Socket() {
        // dont let the exception leave destructor
        try {
            close();
        } catch ( ... ) {
        }
    }

    Socket &operator=( const Socket & ) = delete;
    Socket &operator=( Socket &&other ) {
        swap( other );
        return *this;
    }

    void swap( Socket &other ) {
        using std::swap;

        std::lock( _mRead, _mWrite, other._mRead, other._mWrite );
        std::lock_guard< std::mutex > _r( _mRead, std::adopt_lock );
        std::lock_guard< std::mutex > _w( _mWrite, std::adopt_lock );
        std::lock_guard< std::mutex > _or( other._mRead, std::adopt_lock );
        std::lock_guard< std::mutex > _ow( other._mWrite, std::adopt_lock );

        swap( _fd, other._fd );
    }

    int fd() const {
        std::lock_guard< std::mutex > _( _mRead );
        return _fd;
    }

    bool operator==( const Socket &other ) const {
        return _fd == other._fd;
    }

    bool operator<( const Socket &other ) const {
        return _fd < other._fd;
    }

    bool valid() const {
        std::lock_guard< std::mutex > _( _mRead );
        return _fd != Invalid;
    }

    explicit operator bool() const {
        return valid();
    }

    bool closed() const {
        std::lock_guard< std::mutex > _( _mRead );
        if ( _fd == Invalid )
            return true;
        char buf;
        ssize_t r = ::recv( _fd, &buf, 1, MSG_PEEK );
        return r != 1;
    }

    void close() {
        std::lock( _mRead, _mWrite );
        std::lock_guard< std::mutex > _r( _mRead, std::adopt_lock );
        std::lock_guard< std::mutex > _w( _mWrite, std::adopt_lock );
        if ( _fd != Invalid ) {
            ::close( _fd );
            _fd = Invalid;
        }
    }

    void shutdown( Shutdown how = Shutdown::Both ) {
        std::lock( _mRead, _mWrite );
        std::lock_guard< std::mutex > _r( _mRead, std::adopt_lock );
        std::lock_guard< std::mutex > _w( _mWrite, std::adopt_lock );
        if ( _fd != Invalid )
            ::shutdown( _fd, int( how ) );
    }

    void reset( int fd ) {
        std::lock( _mRead, _mWrite );
        std::lock_guard< std::mutex > _r( _mRead, std::adopt_lock );
        std::lock_guard< std::mutex > _w( _mWrite, std::adopt_lock );
        if ( _fd != Invalid )
            ::close( _fd );
        _fd = fd;
    }

    void receive( Message &message ) const {
        struct msghdr header;
        std::memset( &header, 0, sizeof( header ) );

        std::lock_guard< std::mutex > _( _mRead );
        recv( &message.header(), message.header().size(), true ); // peek
        recv( &message.header(), message.header().size() );

        if ( message.count() ) {
            message.allocateData();

            header.msg_iov = message.ioVector() + 1;
            header.msg_iovlen = message.ioVectorSize() - 1;
            if ( message.length() != message.header().size() + recvmsg( header ) )
                throw DataTransferException( "incomplete on receiving" );
        }
    }

    Message receive() const {
        Message message = Message::dynamicMessage();
        receive( message );
        return message;
    }

    void send( Message &message, bool noThrow = false ) const {
        struct msghdr header;
        std::memset( &header, 0, sizeof( header ) );

        header.msg_iov = message.ioVector();
        header.msg_iovlen = message.ioVectorSize();

        std::lock_guard< std::mutex > _( _mWrite );
        size_t length = sendmsg( header, noThrow );

        if ( !noThrow && message.length() != length )
            throw DataTransferException( "incomplete on sending" );
    }

    ssize_t peek( void *buffer, size_t length ) const {
        std::lock_guard< std::mutex > _( _mRead );

        ssize_t r = ::recv( _fd, buffer, length, MSG_PEEK | MSG_WAITALL );
        if ( r >= 0 )
            return r;

        errnoHandler( "peek (recv)" );
    }

    size_t read( void *buffer, size_t length ) const {
        std::lock_guard< std::mutex > _( _mRead );
        return recv( buffer, length );
    }

    size_t write( const void *buffer, size_t length ) const {
        std::lock_guard< std::mutex > _( _mWrite );
        return send( buffer, length );
    }

    void setNonblocking() {
        int flags;
        if ( ( flags = ::fcntl( _fd, F_GETFL ) ) < 0 )
            throw SystemException( "fcntl" );
        flags |= O_NONBLOCK;
        if ( ::fcntl( _fd, F_SETFL, flags ) < 0 )
            throw SystemException( "fcntl" );
    }

    void setBlocking() {
        int flags;
        if ( ( flags = ::fcntl( _fd, F_GETFL ) ) < 0 )
            throw SystemException( "fcntl" );
        flags &= ~O_NONBLOCK;
        if ( ::fcntl( _fd, F_SETFL, flags ) < 0 )
            throw SystemException( "fcntl" );
    }

    void setTimeout( int timeout ) {
        timeval time;
        time.tv_sec = timeout / 1000;
        time.tv_usec = timeout % 1000;

        if ( ::setsockopt( _fd, SOL_SOCKET, SO_SNDTIMEO, &time, sizeof( time ) ) == -1 )
            throw SystemException( "setsockopt" );
        if ( ::setsockopt( _fd, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof( time ) ) == -1 )
            throw SystemException( "setsockopt" );
    }

private:

    size_t recvmsg( struct msghdr &message ) const {
        ssize_t r = ::recvmsg( _fd, &message, MSG_WAITALL );
        if ( r >= 0 )
            return r;

        errnoHandler( "recvmsg" );
    }

    size_t sendmsg( const struct msghdr &message, bool noThrow = false, bool nonblocking = false ) const {
        int flags = MSG_NOSIGNAL;
        if ( nonblocking )
            flags |= MSG_DONTWAIT;

        ssize_t r = ::sendmsg( _fd, &message, flags );
        if ( noThrow || r >= 0 )
            return r;

        errnoHandler( "sendmsg" );
    }

    size_t recv( void *buffer, size_t length, bool peek = false ) const {
        int flags = MSG_WAITALL;
        if ( peek )
            flags |= MSG_PEEK;

        ssize_t r = ::recv( _fd, buffer, length, flags );
        if ( r >= 0 )
            return r;

        errnoHandler( "recv" );
    }

    size_t send( const void *buffer, size_t length ) const {
        ssize_t r = ::send( _fd, buffer, length, MSG_NOSIGNAL );
        if ( r >= 0 )
            return r;

        errnoHandler( "send" );
    }

    [[noreturn]]
    void errnoHandler( const char *what ) const {

        if ( !_blocking && ( errno == EAGAIN || errno == EWOULDBLOCK ) )
            throw WouldBlock();

        switch ( errno ) {
        case ECONNREFUSED:
        case ENOTCONN:
        case EPIPE:

        //case EAGAIN:
        case EWOULDBLOCK:
            throw ConnectionAbortedException( what );
        default:
            throw SystemException( what );
        }
    }

    int _fd;
    bool _blocking;
    mutable std::mutex _mRead;
    mutable std::mutex _mWrite;
};

inline void swap( Socket &lhs, Socket &rhs ) {
    lhs.swap( rhs );
}

template< typename SocketPtr >
struct Resolution {

    enum _R {
        Timeout,
        Incomming,
        Ready
    };

    Resolution() :
        _r{ Timeout },
        _sockets{}
    {}
    Resolution( _R r, std::vector< SocketPtr > s = std::vector< SocketPtr >() ) :
        _r{ r },
        _sockets{ std::move( s ) }
    {}
    Resolution( const Resolution & ) = default;
    Resolution &operator=( const Resolution & ) = default;

    _R resolution() const {
        return _r;
    }
    std::vector< SocketPtr  > &sockets() {
        return _sockets;
    }
    const std::vector< SocketPtr  > &sockets() const {
        return _sockets;
    }

private:
    _R _r;
    std::vector< SocketPtr > _sockets;
};

struct Network {
    Network( const char *port, bool autobind = true ) :
        _port( port )
    {
        if ( autobind )
            bind();
    }

    template< typename SocketPtr >
    Resolution< SocketPtr > poll( const std::vector< SocketPtr > &toWait, int timeout ) {
        nfds_t nfds = toWait.size() + 1;
        std::vector< PollFD > fds;
        fds.reserve( nfds );

        if ( _ear )
            fds.emplace_back( _ear.fd(), POLLIN );

        for ( const auto &s : toWait )
            fds.emplace_back( s->fd(), POLLIN );

        int r = ::poll( static_cast< pollfd * >( &fds.front() ), nfds, timeout );

        if ( r == -1 )
            throw SystemException( "poll" );
        if ( r == 0 )
            return { Resolution< SocketPtr >::Timeout, {} };

        std::vector< SocketPtr > ready;

        if ( _ear && fds.front().revents ) {
            _incomming.emplace( ::accept( _ear.fd(), nullptr, nullptr ) );
            return { Resolution< SocketPtr >::Incomming, std::move( ready ) };
        }

        ready.reserve( toWait.size() );
        auto selected = fds.begin();
        if ( _ear )
            ++selected;

        for ( const auto &source : toWait ) {
            if ( selected->revents )
                ready.push_back( source );
            ++selected;
        }
        return { Resolution< SocketPtr >::Ready, std::move( ready ) };
    }

    template< typename Iterator >
    Resolution< typename Iterator::value_type > poll( Iterator first, Iterator last, int timeout ) {
        std::vector< typename Iterator::value_type > fds;
        fds.reserve( std::distance( first, last ) );

        std::copy( first, last, std::back_inserter( fds ) );

        return poll( fds, timeout );
    }

    template< typename Iterator, typename L >
    auto poll( Iterator first, Iterator last, L lambda, int timeout )
        -> Resolution< typename std::result_of< L( typename Iterator::value_type ) >::type >
    {
        std::vector< typename std::result_of< L( typename Iterator::value_type ) >::type > fds;
        fds.reserve( std::distance( first, last ) );

        std::transform( first, last, std::back_inserter( fds ), lambda );

        return poll( fds, timeout );
    }


    Socket connect( const char *host, bool nonblocking = false ) {
        int s;
        struct addrinfo hints;
        struct addrinfo *current, *rp;

        std::memset( &hints, 0, sizeof( hints ) );
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = 0;
        hints.ai_protocol = 0;

        if ( ::getaddrinfo( host, port(), &hints, &current ) )
            throw std::runtime_error( "cannot resolve address" );

        for ( rp = current; rp; rp = rp->ai_next ) {
            s = ::socket( rp->ai_family, rp->ai_socktype | SOCK_NONBLOCK, rp->ai_protocol );
            if ( s == -1 )
                continue;

            int r = ::connect( s, rp->ai_addr, rp->ai_addrlen );
            if ( r == 0 )
                break;
            if ( r == -1 && errno == EINPROGRESS ) {
                PollFD toWait( s, POLLOUT );
                int r = ::poll( &toWait, 1, settings::timeout );
                if ( r == -1 )
                    throw SystemException( "poll" );
                if ( r > 0 ) {
                    int isError;
                    socklen_t l = sizeof( int );
                    if ( ::getsockopt( s, SOL_SOCKET, SO_ERROR, &isError, &l ) == -1 )
                        throw SystemException( "getsockopt" );
                    if ( !isError )
                        break;
                }
                else
                    throw ConnectionTimeout( host );
            }
            close( s );
        }
        ::freeaddrinfo( current );

        if ( !rp )
            throw SystemException( ECONNREFUSED, "connect" );

        Socket result{ s };
        if ( !nonblocking ) {
            result.setBlocking();
            result.setTimeout( settings::timeout );
        }

        return result;
    }

    std::string peerAddress( const Socket &s ) const {
        struct sockaddr_storage addr;
        socklen_t len = sizeof( addr );

        if ( ::getpeername( s.fd(), reinterpret_cast<struct sockaddr *>( &addr ), &len ) )
            throw SystemException( "getpeername" );

        char buf[ 64 ];

        switch ( addr.ss_family ) {
        case AF_INET:
            inet_ntop( AF_INET, &( reinterpret_cast< struct sockaddr_in * >( &addr )->sin_addr ), buf, 64 );
            break;
        case AF_INET6:
            inet_ntop( AF_INET6, &( reinterpret_cast< struct sockaddr_in6 * >( &addr )->sin6_addr ), buf, 64 );
            break;
        default:
            return "unknown family";
        }

        return buf;
    }

    std::string selfAddress() const {
        struct sockaddr_storage addr;
        socklen_t len = sizeof( addr );

        if ( ::getsockname( _ear.fd(), reinterpret_cast<struct sockaddr *>( &addr ), &len ) )
            throw SystemException( "getsockname" );

        char buf[ 64 ];

        switch ( addr.ss_family ) {
        case AF_INET:
            inet_ntop( AF_INET, &( reinterpret_cast< struct sockaddr_in * >( &addr )->sin_addr ), buf, 64 );
            break;
        case AF_INET6:
            inet_ntop( AF_INET6, &( reinterpret_cast< struct sockaddr_in6 * >( &addr )->sin6_addr ), buf, 64 );
            break;
        default:
            return "unknown family";
        }

        return buf;
    }

    Socket incomming( bool nonblocking = false ) {
        if ( _incomming.empty() )
            throw std::runtime_error( "no incomming connections" );
        Socket r{ std::move( _incomming.front() ) };
        if ( nonblocking )
            r.setNonblocking();
        else
            r.setTimeout( settings::timeout );
        _incomming.pop();
        return r;
    }

    const char *port() const {
        return _port;
    }

    void unbind() {
        _ear.close();
    }

    static std::pair< Socket, Socket > socketPair() {
        int socks[ 2 ];
        if ( -1 == ::socketpair( AF_UNIX, SOCK_STREAM, 0, socks ) )
            throw SystemException( "socketpair" );

        return { Socket{ socks[ 0 ] }, Socket{ socks[ 1 ] } };
    }

private:

    void bind() {
        struct addrinfo hints;
        struct addrinfo *result, *rp;

        std::memset( &hints, 0, sizeof( hints ) );
        hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
        hints.ai_protocol = 0;          /* Any protocol */
        hints.ai_canonname = nullptr;
        hints.ai_addr = nullptr;
        hints.ai_next = nullptr;

        if ( ::getaddrinfo( nullptr, port(), &hints, &result ) )
            throw SystemException( "getaddrinfo" );

        for ( rp = result; rp; rp = rp->ai_next ) {

            int s = ::socket( rp->ai_family, rp->ai_socktype, rp->ai_protocol );
            if ( s == -1 )
                continue;

            if ( !::bind( s, rp->ai_addr, rp->ai_addrlen ) ) {
                _ear.reset( s );
                break;
            }

            close( s );
        }
        ::freeaddrinfo( result );

        if ( !rp )
            throw SystemException( "bind" );
        ::listen( _ear.fd(), 25 );
    }

    const char *_port;
    Socket _ear;
    std::queue< Socket > _incomming;
};


} // namespace net
} // namespace brick

namespace brick_test {
namespace net {

using namespace brick::net;

struct MessageTest {
    TEST(preallocated) {
        Socket in, out;
        std::tie( in, out ) = Network::socketPair();

        std::string outBuffer = "karel";
        char inBuffer[ 6 ] = {};

        Message o;
        o << outBuffer;

        Message i;
        i.add( inBuffer, 5 );

        out.send( o );
        in.receive( i );

        ASSERT_EQ( std::string( outBuffer ), std::string( inBuffer ) );
    }

    TEST(string) {
        std::string out( "test string" );
        std::string in;
        Message m;
        m << out;

        m.storeTo() >> in;
        ASSERT_EQ( in, out );
    }

    TEST(dynamic) {
        std::vector< std::string > data = {
            "test1",
            "next",
            "brick",
            "divine"
        };

        Socket in, out;
        std::tie( in, out ) = Network::socketPair();

        Message o;

        for ( auto &s : data )
            o << s;
        out.send( o );

        Message m = in.receive();

        auto mi = m.begin();
        auto di = data.begin();
        for ( ; mi != m.end() && di != data.end(); ++mi, ++di ) {
            std::string str( mi->data(), mi->size() );
            ASSERT_EQ( str, *di );
        }

        ASSERT( mi == m.end() );
        ASSERT( di == data.end() );
    }

    TEST( tagged ) {
        Message mOut;
        mOut.tag( 10 );

        Socket in, out;
        std::tie( in, out ) = Network::socketPair();

        out.send( mOut );
        Message mIn = in.receive();
        ASSERT_EQ( 10, mIn.tag() );
        ASSERT_EQ( 0, mIn.count() );
    }
};

} // namespace net
} // namespace brick_test

#endif
