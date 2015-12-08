#include <stdint.h>
#include <vector>
#include <string>
#include <algorithm>
#include <initializer_list>

#include <brick-net.h>

#ifndef MESSAGE_H
#define MESSAGE_H

// CATEGORY
enum class MessageType : uint8_t {
    Data = 128,
    Control,
    Output
};

// TAG (Output)
enum class Output {
    Standard,
    Error
};

// TAG (Control)
enum class Code {
    NoOp = 0,
    OK = 6,
    Success,
    Refuse,
    Enslave,
    ID, // %D // dead
    ConnectTo, // %S %D
    DataLine, // %D (%D)
    Join,
    Disconnect,
    Peers, // %D
    Leave,
    Grouped,
    PrepareToLeave,
    Shutdown,
    ForceShutdown,
    CutRope,
    InitialData,
    Run,
    Start,
    Done,


    Error = 32,
    Renegade,

    Status = 64,
};

const char *codeToString( Code code );

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

struct ResponseException : brick::net::NetException {
    ResponseException( std::initializer_list< Code > expected, Code given ) {
        _what += "invalid response; expected: { ";
        for ( Code c : expected ) {
            _what += codeToString( c );
            _what += " ";
        }
        _what += "} given: ";
        _what += codeToString( given );
    }
    const char *what() const noexcept override {
        return _what.c_str();
    }
private:
    std::string _what;
};

struct SendAllException : brick::net::NetException {
    SendAllException() :
        _what{ "sendAll failed - not all recepients received the message" }
    {}
    const char *what() const noexcept override {
        return _what;
    }
private:
    const char *_what;
};

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

    friend brick::net::InputMessage &operator>>( brick::net::InputMessage &message, Address &address ) {
        message.add( address._value, SIZE );
        return message;
    }
    friend brick::net::OutputMessage &operator<<( brick::net::OutputMessage &message, const Address &address ) {
        message.add( address._value, Address::SIZE );
        return message;
    }

    char _value[ SIZE ];
};

#endif
