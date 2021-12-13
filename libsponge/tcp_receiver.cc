#include "tcp_receiver.hh"

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();
    seq = header.seqno;
    if (!isn && header.syn) {
        isn = 1, Syn = seq;
        len = seg.length_in_sequence_space();
        if (len > 1 || header.fin) 
            _reassembler.push_substring(move(string(seg.payload().str())), 0, header.fin);
        return;
    }
    if (isn) {
        size_t res = unwrap(seq, Syn, _reassembler.last_reassembled() + 1);
        if (header.fin) isn = 2;
        _reassembler.push_substring(move(string(seg.payload().str())), res - 1, header.fin);
        if (len == res) {
            len = res + seg.length_in_sequence_space();
            while (um.count(len)) len = um[len];
            if (isn == 1 && len - res > stream_out().buffer_size()) {
                len = stream_out().buffer_size() + res;
                um[len] = res + seg.length_in_sequence_space();
            }
        }
        else um[res] = res + seg.length_in_sequence_space();
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    optional<WrappingInt32> res;
    if (isn) res = wrap(len, Syn);
    return res;
}

size_t TCPReceiver::window_size() const {
    return stream_out().remaining_capacity();
}
