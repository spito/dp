#ifndef BRICK_COMMON_H
#define BRICK_COMMON_H

namespace brick {
namespace common {

struct Comparable {
    using CompareResult = bool;
};
struct Orderable {
    using OrderResult = bool;
};

template< typename T >
typename T::CompareResult operator!=( const T &lhs, const T &rhs ) {
    return !(lhs == rhs );
}

template< typename T >
typename T::OrderResult operator<=( const T &lhs, const T &rhs ) {
    return !( rhs < lhs );
}

template< typename T >
typename T::OrderResult operator>( const T &lhs, const T &rhs ) {
    return ( rhs < lhs );
}

template< typename T >
typename T::OrderResult operator>=( const T &lhs, const T &rhs ) {
    return !( lhs < rhs );
}

} // namespace common
} // namespace brick

#endif
