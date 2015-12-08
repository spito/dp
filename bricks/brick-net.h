#include <memory>
#include <atomic>
#include <stdexcept>
#include <iterator>
#include <string>
#include <cstring>
#include <chrono>
#include <tuple>
#include <future>
#include <algorithm>
#include <type_traits>
#include <numeric>

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
#include <brick-types.h>
#include <brick-unittest.h>

#ifndef BRICK_NET_H
#define BRICK_NET_H

namespace brick {
namespace net {

namespace settings {
    // set default timeout to sockets
    static constexpr int timeout =
//#ifdef DEBUG
//        8 * 60 * 1000
//#else
        8 * 60  * 1000
//#endif
    ;
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
    DataTransferException( const char *what, int expected, int actual )
    {
        _what += "Incomplete data transfer when ";
        _what += what;
        _what += ": expected=" + std::to_string( expected );
        _what += " actual=" + std::to_string( actual );
    }

    const char *what() const noexcept override {
        return _what.c_str();
    }
private:
    std::string _what;
};

struct WouldBlock : NetException {
    const char *what() const noexcept override { return "would block"; }
};

struct BadMutex : NetException {
    const char *what() const noexcept override { return "bad mutex"; }
};

template< typename It >
struct Adaptor {
    Adaptor( It b, It e ) :
        _begin( b ),
        _end( e )
    {}
    It begin() {
        return _begin;
    }
    It end() {
        return _end;
    }
private:
    It _begin;
    It _end;
};

template< typename It >
Adaptor< It > make_adaptor( It begin, It end ) {
    return Adaptor< It >( begin, end );
}

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
        Header( const Header & ) = default;
        Header( Header &&other ) :
            Header( other )
        {
            other.clear();
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
        size_t head() const {
            return HEAD;
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
    static_assert(
        sizeof( IOvector ) == sizeof( iovec ),
        "wrong length of C++ wrapper over iovec" );

public:
    Message() :
        _data{ { &_header, _header.size() } }
    {}
    template< typename T = int >
    T category() const {
        return static_cast< T >( _header.category );
    }
    template< typename T >
    void category( T c ) {
        _header.category = static_cast< uint8_t >( c );
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

    size_t count() const {
        return _header.count;
    }

    bool full() const {
        return _header.count == Header::SEGMENTS;
    }
    Header &header() {
        return _header;
    }
    const Header &header() const {
        return _header;
    }

    bool add( void *data, uint32_t length ) {
        if ( full() && _data.size() == Header::SEGMENTS + 1 )
            return false;
        _data.emplace_back( data, length );
        // fill unknown positions
        // one for header
        if ( _data.size() > _header.count + 1 ) {
            _header.count = _data.size() - 1;
            updatedHeaderSize();
        }
        _header.segments[ _data.size() - 2 ] = length;
        return true;
    }

    void count( int c ) {
        _header.count = c;
    }
    IOvector *vector() {
        return _data.data();
    }
    size_t bytes() const {
        return std::accumulate(
            _data.begin(),
            _data.end(),
            size_t( 0 ),
            []( size_t sum, const IOvector &v ) {
                return sum + v.size();
            } );
    }
    void clear() {
        _header.clear();
        _data.clear();
        _data.emplace_back( &_header, _header.size() );
    }
protected:
    std::vector< IOvector > &data() {
        return _data;
    }

    uint32_t segment( int i ) {
        return _header.segments[ i ];
    }
    void updatedHeaderSize() {
        _data.front().size( _header.size() );
    }

private:

    Header _header;
    std::vector< IOvector > _data;
};

struct OutputMessage : Message {
    template< typename T >
    OutputMessage( T c ) {
        category( c );
    }

    OutputMessage( const OutputMessage & ) = default;
    OutputMessage( OutputMessage &&other ) = default;
    OutputMessage &operator=( const OutputMessage & ) = default;

    explicit operator bool() const noexcept {
        return !full();
    }

    bool add( void *data, uint32_t length ) {
        return Message::add( data, length );
    }
    bool add( const void *data, uint32_t length ) {
        _cache.emplace_back( new char[ length ] );
        const char *c = static_cast< const char * >( data );
        std::copy( c, c + length, _cache.back().get() );
        return Message::add( _cache.back().get(), length );
    }

    template< typename T >
    auto operator<<( T &data )
        -> typename std::enable_if<
            std::is_trivially_copyable< T >::value,
            OutputMessage &
        >::type
    {
        add( &data, sizeof( T ) );
        return *this;
    }

    template< typename T >
    auto operator<<( const T &data )
        -> typename std::enable_if<
            std::is_trivially_copyable< T >::value,
            OutputMessage &
        >::type
    {
        add( &data, sizeof( T ) );
        return *this;
    }

    OutputMessage &operator<<( std::string &data ) {
        add( &data.front(), data.size() );
        return *this;
    }

    template< typename T >
    auto operator<<( std::vector< T > &data )
        -> typename std::enable_if<
            std::is_trivially_copyable< T >::value,
            OutputMessage &
        >::type
    {
        add( data.data(), data.size() * sizeof( T ) );
        return *this;
    }
private:
    std::vector< std::unique_ptr< char > > _cache;
};

struct InputMessage : Message {
    InputMessage() = default;
    InputMessage( const InputMessage & ) = default;
    InputMessage( InputMessage && ) = default;

    template< typename T >
    auto operator>>( T &data )
        -> typename std::enable_if<
            std::is_trivially_copyable< T >::value,
            InputMessage &
        >::type
    {
        add( &data, sizeof( T ) );
        return *this;
    }

    InputMessage &operator>>( std::string &data ) {
        add( &data, 0 );
        return *this;
    }

    template< typename A >
    void allocateData( A allocator ) {
        // empty buffer
        if ( data().size() == 1 ) {
            data().reserve( count() + 1 );

            for ( int i = 0; i < count(); ++i )
                data().emplace_back(
                    allocator( segment( i ) ),
                    segment( i )
                );
        }
        // only prepare strings
        else {
            uint32_t *segmentSize = header().segments;
            for ( IOvector &v : make_adaptor( data().begin() + 1, data().end() ) ) {
                if ( v.size() == 0 ) {// handle string separately
                    std::string *string = v.data< std::string >();
                    string->reserve( *segmentSize );
                    string->assign( *segmentSize, ' ' );
                    v.data( &string->front() );
                }

                if ( *segmentSize != v.size() )
                    v.size( *segmentSize );
                ++segmentSize;
            }
        }
        updatedHeaderSize();
    }

    template< typename T, typename C >
    void cleanup( C cleaner ) {
        for ( IOvector &v : make_adaptor( data().begin() + 1, data().end() ) ) {
            cleaner( v.data< T >() );
        }
    }

    template< typename > class Dummy;

    template< typename T = char, typename P = Dummy< T > >
    void process( P processor ) {
        for ( IOvector &v : make_adaptor( data().begin() + 1, data().end() ) ) {
            processor( v.data< T >(), v.size() );
        }
    }
};

enum class Access : bool {
    Read,
    Write
};

struct Socket : common::Comparable, common::Orderable {

    enum { Invalid = -1 };

    Socket() :
        _fd{ Invalid }
    {}
    explicit Socket( int fd ) :
        _fd{ fd }
    {}
    Socket( const Socket & ) = delete;
    Socket( Socket &&other ) :
        _fd{ Invalid }
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

        swap( _fd, other._fd );
    }

    int fd() const {
        return _fd;
    }

    friend bool operator==( const Socket &lhs, const Socket &rhs ) {
        return lhs._fd == rhs._fd;
    }

    friend bool operator<( const Socket &lhs, const Socket &rhs ) {
        return lhs._fd < rhs._fd;
    }

    bool valid() const {
        return _fd != Invalid;
    }

    explicit operator bool() const {
        return valid();
    }

    bool closed() const {
        if ( _fd == Invalid )
            return true;
        char buf;
        return 1 != ::recv( _fd, &buf, 1, MSG_PEEK );
    }

    void close() {
        if ( _fd != Invalid ) {
            ::close( _fd );
            _fd = Invalid;
        }
    }

    void shutdown( Shutdown how = Shutdown::Both ) {
        if ( _fd != Invalid )
            ::shutdown( _fd, int( how ) );
    }

    void reset( int fd ) {
        if ( _fd != Invalid )
            ::close( _fd );
        _fd = fd;
    }

    void peek( InputMessage &message ) const {
        recv( &message.header(), message.header().size(), true );
    }

    void receive( InputMessage &message ) const {
        receive( message, []( size_t s ) { return new char[ s ]; } );
    }

    void receiveHeader( InputMessage &message ) const {
        receive( message );
        message.cleanup< char >( []( char *d ) {
            delete[] d;
        } );
    }

    template< typename A >
    void receive( InputMessage &message, A allocator ) const {
        struct msghdr header;
        std::memset( &header, 0, sizeof( header ) );

        recv( &message.header(), message.header().head(), true ); // peek
        recv( &message.header(), message.header().size() );

        if ( message.count() ) {
            message.allocateData( allocator );

            header.msg_iov = message.vector() + 1;// skip the header
            header.msg_iovlen = message.count();

            ssize_t received = recvmsg( header );

            if ( message.bytes() != message.header().size() + received )
                throw DataTransferException(
                    "receive",
                    message.bytes(),
                    message.header().size() + received
                );
        }
    }

    void send( OutputMessage &message, bool noThrow = false ) const {
        struct msghdr header;
        std::memset( &header, 0, sizeof( header ) );

        header.msg_iov = message.vector();
        header.msg_iovlen = message.count() + 1;

        size_t sent = sendmsg( header, noThrow );

        if ( !noThrow && message.bytes() != sent )
            throw DataTransferException(
                "send",
                message.bytes(),
                sent
            );
    }

    ssize_t peek( void *buffer, size_t length ) const {
        ssize_t r = ::recv( _fd, buffer, length, MSG_PEEK | MSG_WAITALL );
        if ( r >= 0 )
            return r;

        errnoHandler( "peek (recv)" );
    }

    size_t read( void *buffer, size_t length ) const {
        return recv( buffer, length );
    }

    size_t write( const void *buffer, size_t length ) const {
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

        if ( errno == EAGAIN || errno == EWOULDBLOCK )
            throw WouldBlock();

        switch ( errno ) {
        case ECONNREFUSED:
        case ENOTCONN:
        case EPIPE:
            throw ConnectionAbortedException( what );
        default:
            throw SystemException( what );
        }
    }

    int _fd;
};

inline void swap( Socket &lhs, Socket &rhs ) {
    lhs.swap( rhs );
}

struct Network {
    Network( const char *port, bool autobind = true ) :
        _port( port )
    {
        if ( autobind )
            bind();
    }


    template<
        typename S, // SocketPtr
        typename I, // Incoming
        typename P  // Process
    >
    int poll( const std::vector< S > &toWait, I incoming, P process, bool listen, int timeout ) {
        std::vector< PollFD > fds;

        fds.reserve( toWait.size() + 1 );

        if ( listen && _ear )
            fds.emplace_back( _ear.fd(), POLLIN );

        for ( const auto &s : toWait )
            fds.emplace_back( s->fd(), POLLIN );

        int r = ::poll( static_cast< pollfd * >( &fds.front() ), fds.size(), timeout );

        if ( r == -1 )
            throw SystemException( "poll" );
        if ( r == 0 )
            return r;

        if ( listen && _ear && fds.front().revents ) {
            Socket newSocket{ ::accept( _ear.fd(), nullptr, nullptr ) };
            newSocket.setTimeout( settings::timeout );
            incoming( std::move( newSocket ) );
        }

        auto selected = fds.begin();
        if ( listen && _ear )
            ++selected;

        for ( const auto &s : toWait ) {
            if ( selected->revents ) {
                if ( !process( s ) )
                    break;
            }
            ++selected;
        }
        return r;
    }

    Socket connect( const char *host, bool noThrow = false ) {
        int s;
        struct addrinfo hints;
        struct addrinfo *current, *rp;

        std::memset( &hints, 0, sizeof( hints ) );
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = 0;
        hints.ai_protocol = 0;

        if ( ::getaddrinfo( host, port(), &hints, &current ) ) {
            if ( noThrow )
                return Socket{};
            throw SystemException( "cannot resolve address" );
        }

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

        if ( !rp ) {
            if ( noThrow )
                return Socket{};
            throw SystemException( ECONNREFUSED, "connect" );
        }

        Socket result{ s };
        result.setBlocking();
        result.setTimeout( settings::timeout );

        return result;
    }

    std::string peerAddress( const Socket &s ) const {
        struct sockaddr_storage addr;
        socklen_t len = sizeof( addr );

        if ( ::getpeername( s.fd(), reinterpret_cast< struct sockaddr * >( &addr ), &len ) )
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

        if ( ::getsockname( _ear.fd(), reinterpret_cast< struct sockaddr * >( &addr ), &len ) )
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
            int on = 1;
            if ( ::setsockopt( s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof( on ) ) )
                throw SystemException( "setsockopt" );

            if ( !::bind( s, rp->ai_addr, rp->ai_addrlen ) ) {
                _ear.reset( s );
                break;
            }

            close( s );
        }
        ::freeaddrinfo( result );

        if ( !rp )
            throw SystemException( "bind" );
        if ( ::listen( _ear.fd(), 25 ) )
            throw SystemException( "listen" );
    }

    const char *_port;
    Socket _ear;
};


struct Redirect {
    using FD = int;
    enum { Invalid = -1 };

    Redirect( FD source ) :
        Redirect( source, ::open( "/dev/null", O_RDWR ) )
    {}

    Redirect( FD source, FD target ) :
        _current( source ),
        _original( ::dup( source ) )
    {
        if ( _original == Invalid )
            throw SystemException( "dup" );

        if ( target == Invalid )
            return;

        if ( ::close( _current ) == -1 )
            throw SystemException( "close" );

        if ( ::dup2( target, _current ) == -1 )
            throw SystemException( "dup2" );

        if ( ::close( target ) == -1 )
            throw SystemException( "close" );
    }

    ~Redirect() {
        ::close( _current );
        ::dup2( _original, _current );
        _current = Invalid;
        ::close( _original );
        _original = Invalid;
    }
private:
    FD _current;
    FD _original;
};

struct Redirector {
    using FD = int;
    enum { Invalid = -1 };

    template< typename Handler >
    Redirector( FD source, Handler h, int bufferSize = 4 * 1024 ) :
        _output( source ),
        _original( ::dup( source ) ),
        _bufferSize( bufferSize )
    {
        if ( _original == Invalid )
            throw SystemException( "dup" );

        FD p[2];
        if ( ::pipe( p ) == -1 )
            throw SystemException( "pipe" );
        _input = p[ 0 ];

        if ( ::dup2( p[ 1 ], _output ) == -1 )
            throw SystemException( "dup2" );

        if ( ::close( p[ 1 ] ) == -1 )
            throw SystemException( "close" );

        _future = std::async( std::launch::async, [h,this] {
            executive( h );
        } );
    }
    Redirector( const Redirector & ) = delete;
    Redirector &operator=( const Redirector & ) = delete;

    ~Redirector() {
        try {
            stop();
        } catch (...) {
        }
    }

    void stop() {
        if ( _output == Invalid )
            return;
        ::close( _output );

        types::Defer d( [&] {
            ::dup2( _original, _output );
            _output = Invalid;
            ::close( _original );
            _original = Invalid;
        } );

        _future.get();
    }

private:

    template< typename Handler >
    void executive( Handler h ) {
        std::unique_ptr< char[] > buffer( new char[ _bufferSize ] );

        ssize_t r;
        while ( ( r = ::read( _input, buffer.get(), _bufferSize ) ) > 0 ) {
            h( buffer.get(), r );
        }
        if ( r == -1 )
            throw SystemException( "read" );
        ::close( _input );
        _input = Invalid;
    }

    FD _input;
    FD _output;
    FD _original;
    int _bufferSize;
    std::future< void > _future;
};

} // namespace net
} // namespace brick

namespace brick_test {
namespace net {

using namespace brick::net;

struct Message {
    TEST( preallocated ) {
        Socket in, out;
        std::tie( in, out ) = Network::socketPair();

        std::string outBuffer = "karel";
        char inBuffer[ 6 ] = {};

        OutputMessage o( 0 );
        o << outBuffer;

        InputMessage i;
        i.add( inBuffer, 5 );

        out.send( o );
        in.receive( i );

        ASSERT_EQ( outBuffer, std::string( inBuffer ) );
    }

    TEST( string ) {
        Socket in, out;
        std::tie( in, out ) = Network::socketPair();

        std::string outBuffer = "karel";
        std::string inBuffer;

        OutputMessage o( 0 );
        o << outBuffer;

        InputMessage i;
        i >> inBuffer;

        out.send( o );
        in.receive( i );

        ASSERT_EQ( outBuffer, inBuffer );
    }

    TEST( peek ) {
        //::alarm( 10 );
        Socket in, out;
        std::tie( in, out ) = Network::socketPair();

        std::string outBuffer = "karel";
        std::string inBuffer;

        OutputMessage o( 0 );
        o << outBuffer;

        InputMessage i;
        out.send( o );
        in.peek( i );

        i >> inBuffer;
        in.receive( i );

        ASSERT_EQ( outBuffer, inBuffer );
    }

    TEST( dynamic ) {
        std::vector< std::string > data = {
            "test1",
            "next",
            "brick",
            "divine"
        };

        Socket in, out;
        std::tie( in, out ) = Network::socketPair();

        OutputMessage o( 0 );

        for ( auto &s : data )
            o << s;
        out.send( o );

        InputMessage m;
        in.receive( m, []( size_t s ) { return new char[ s ]; } );

        auto di = data.begin();
        m.process< char >( [&]( const char *msg, size_t length ) {
            ASSERT( di != data.end() );
            std::string str( msg, length );
            ASSERT_EQ( str, *di );
            ++di;
        } );
        ASSERT( di == data.end() );

        m.cleanup< char >( []( char *d ) {
            delete[] d;
        } );
    }

    TEST( integer ) {
        OutputMessage mOut( 0 );
        Socket in, out;
        std::tie( in, out ) = Network::socketPair();
        int time = 1258;
        int rTime = 0;
        mOut << time;
        out.send( mOut );

        InputMessage mIn;
        mIn >> rTime;
        in.receive( mIn );

        ASSERT_EQ( rTime, time );
    }

    TEST( tagged ) {
        OutputMessage mOut( 0 );
        mOut.tag( 10 );

        Socket in, out;
        std::tie( in, out ) = Network::socketPair();

        out.send( mOut );

        InputMessage mIn;
        in.receive( mIn );

        ASSERT_EQ( 10, mIn.tag() );
        ASSERT_EQ( 0, mIn.count() );
    }

    TEST( allocation ) {
        Socket in, out;
        std::tie( in, out ) = Network::socketPair();

        OutputMessage o( 0 );
        o.tag( 10 );
        o.add( "abc", 3 );
        o.add( "def", 3 );
        o.add( "xkcd", 4 );

        out.send( o );
        InputMessage i;
        in.receiveHeader( i );
        ASSERT_EQ( 10, i.tag() );

        i.clear();

        o.tag( 12 );
        out.send( o );
        in.receiveHeader( i );
        ASSERT_EQ( 12, i.tag() );
    }

    TEST( overfilled ) {
        Socket in, out;
        std::tie( in, out ) = Network::socketPair();

        OutputMessage o( 0 );
        o.tag( 10 );

        out.send( o );

        int data;
        InputMessage i;
        i >> data;
        in.receive( i );
        ASSERT_EQ( 10, i.tag() );
    }

    TEST( redirector ) {
        ::alarm( 10 );
        int p[2];
        ::pipe( p );
        char buffer[ 20 ] = {};

        Redirector redirector( p[ 1 ], []( const char *m, size_t length ) {
            ASSERT_EQ( std::string( m, length ), "hello dolly!" );
        } );

        ASSERT_EQ( ::write( p[ 1 ], "hello dolly!", 12 ), 12 );
        redirector.stop();
        ASSERT_EQ( ::write( p[ 1 ], "hello!", 6 ), 6 );
        ASSERT_EQ( ::read( p[ 0 ], buffer, 20 ), 6 );
        ::close( p[ 1 ] );
        ASSERT_EQ( ::read( p[ 0 ], buffer, 20 ), 0 );
    }
};

} // namespace net
} // namespace brick_test

#endif
