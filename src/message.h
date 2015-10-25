#include <stdint.h>
#include <vector>
#include <string>
#include <algorithm>

#include <brick-net.h>

#ifndef MESSAGE_H
#define MESSAGE_H

enum class MessageType : uint8_t {
    Data = 128,
    ExpandData,
    Control,
};

struct NetworkException : brick::net::NetException {
    NetworkException() :
        _what{ "internal network problem" }
    {}

    NetworkException( std::string reason ) :
        _what{ "internal network problem: " + std::move( reason ) }
    {}

    const char *what() const noexcept override {
        return _what.c_str();
    }
private:
    std::string _what;
};

enum class Code : int {
    NOOP,
    ACK = 6,
    SUCCESS,
    FAILED,
    REFUSE,
    ENSLAVE,
    ID, // %D // dead
    CONNECT, // %S %D
    JOIN,
    DISCONNECT,
    PEERS, // %D
    LEAVE,
    GROUPED,
    PREPARE,
    SHUTDOWN,
    CUTROPE,
    INITDATA,
    RUN,
    START,
    DONE,


    ERROR = 32,
    RENEGADE,

    TABLE = 64, // debug only
};

enum class Tags : int {
    OUTPUT,
    DATA
};

const char *codeToString( Code code );

struct Address {
private:
    static constexpr const size_t SIZE = 32;
    static constexpr const size_t LENGTH = SIZE - 1;
public:
    Address() = default;
    Address( const Address & ) = default;
    Address( const std::string &address ) {
        set( address );
    }
    Address( const char *address ) {
        set( address );
    }

    Address &operator=( const Address &other ) {
        std::memcpy( _value, other._value, SIZE );
        return *this;
    }
    Address &operator=( const std::string &address ) {
        set( address );
        return *this;
    }
    Address &operator=( const char *address ) {
        set( address );
        return *this;
    }

    const char *value() const {
        return _value;
    }

    constexpr unsigned size() const {
        return SIZE;
    }

private:

    void set( const std::string &address ) {
        size_t length = address.size();
        if ( length > LENGTH )
            length = LENGTH;
        std::memset( _value + length, 0, SIZE - length );
        std::memcpy( _value, address.c_str(), length );
    }
    void set( const char *address ) {
        size_t length = std::strlen( address );
        if ( length > LENGTH )
            length = LENGTH;
        std::memset( _value + length, 0, SIZE - length );
        std::memcpy( _value, address, length );
    }

    friend brick::net::Message &operator>>( brick::net::Message &message, Address &address ) {
        brick::net::IOvector *v = message.read();
        if ( !v )
            throw NetworkException( "missing address item" );
        if ( v->size() != Address::SIZE )
            throw NetworkException( "not an address object" );
        std::memcpy( address._value, v->data(), Address::SIZE );
        return message;
    }
    friend brick::net::Message &operator<<( brick::net::Message &message, Address &address ) {
        message.add( address._value, Address::SIZE );
        return message;
    }
    
    char _value[ SIZE ];
};

#endif
