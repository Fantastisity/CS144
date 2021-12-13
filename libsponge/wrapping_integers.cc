#include "wrapping_integers.hh"

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    return WrappingInt32(isn + n);
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint32_t check = (checkpoint + isn.raw_value()) % (1ul << 32), cur = n.raw_value();
    uint64_t a, b;
    if (cur <= check) {
        a = check - cur;
        b = cur + (1ul << 32) - check;
        return a < b ? (a > checkpoint ? ((1ul << 32) - a + checkpoint) : checkpoint - a) : checkpoint + b;
    } 
    a = cur - check;
    b = check + (1ul << 32) - cur;
    return a < b ? a + checkpoint : (b > checkpoint ? ((1ul << 32) - b + checkpoint) : checkpoint - b);
}
