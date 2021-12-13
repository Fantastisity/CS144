#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>
#include <iostream>
#include <cstdio>
#include <random>
#include <map>

using namespace std;

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;
    size_t wind = 1, syn = 0, conseq = 0, Ack = 0, unack = 1, len = 0, tot_ms = 0, prev_mx = 0, _eof = 0;
    string cur{};
    bool backoff = 1;
    TCPSegment seg{};
    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};
    map<size_t, TCPSegment> chq{};

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout, rto;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};

  public:
    // retx_timeout: initial amount of time to wait before retransmitting the oldest outstanding segment
    // fixed_isn: Initial Sequence Number
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {}) : 
              _isn(fixed_isn.value_or(WrappingInt32{random_device()()})), 
              _initial_retransmission_timeout{retx_timeout}, 
              rto(retx_timeout), _stream(capacity) {}

    bool resend = 0;
    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const { return unack; };

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const { return conseq; };

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}
    TCPSegment make_seg(bool syn_, bool fin, bool rst, WrappingInt32 seq_no, 
                        WrappingInt32 ack_no, bool pld = 0, Buffer bf = Buffer());
    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }
    
    inline bool fully_acked() { return _eof == 2; }
    
    bool check_syn(WrappingInt32 remote_isn) {
        if (!syn) {
            syn = 1;
            _isn = remote_isn;
            _next_seqno = unack = 1;
            chq[next_seq()] = make_seg(1, 0, 0, _isn, next_seqno());
            return 1;
        }
        return 0;
    }
    
    void send_empty_seg(int R = 0, int S = 0) {
        WrappingInt32 tmp = next_seqno();
        if (S) ++_next_seqno;
        _segments_out.push(make_seg(S, 0, R, tmp, next_seqno() , 0));
    }
    
    uint64_t next_seq(int l = 0) const { return _next_seqno + l + _isn.raw_value(); }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
