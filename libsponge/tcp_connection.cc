#include "tcp_connection.hh"
#include <ostream>
size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().buffer_size(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return ms_elas - seg_recv; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!_active) return;
    seg_recv = ms_elas;
    if (seg.header().rst) terminate();
    else {
        if (_receiver.ackno().has_value()) {
            if (!seg.length_in_sequence_space() && seg.header().seqno == _receiver.ackno().value() - 1) 
                _sender.send_empty_seg(), forward(1);
            else if (seg.header().seqno >= _receiver.get_syn()) _receiver.segment_received(seg);
        } else if (!seg.header().syn && seg.header().ack) return;
        if (seg.header().syn || seg.header().fin) {
            _receiver.segment_received(seg);
            if (_linger_after_streams_finish && seg.header().fin && !fin_sent) _linger_after_streams_finish = 0;
            TCPSegment tmp = seg;
            tmp.header().ackno = tmp.header().seqno + 1;
            tmp.header().fin = 0;
            tmp.header().syn = _sender.check_syn(seg.header().seqno);
            tmp.header().ack = 1;
            tmp.header().win = _receiver.window_size();
            _segments_out.push(tmp);
        }
        if (seg.header().ack) {
            _sender.ack_received(seg.header().ackno, seg.header().win);
            if (!seg.header().syn && !seg.header().fin) {
                if (!_sender.stream_in().input_ended() && _sender.segments_out().size())
                    _sender.fill_window(), forward(_sender.segments_out().size());
                else if (_sender.stream_in().buffer_empty() && seg.length_in_sequence_space()) 
                     _sender.send_empty_seg(), forward(1);
            }
        }
    }
}

size_t TCPConnection::write(const string &data) {
    size_t res = _sender.stream_in().write(data);
    _sender.fill_window(), forward(_sender.segments_out().size());
    return res;
}

void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!_active) return;
    ms_elas += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.resend) {
        if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
            terminate(1); return;
        }
        forward(1);
    }
    if (_sender.fully_acked() && !_receiver.unassembled_bytes() && _receiver.stream_out().input_ended()) {
        if (!_linger_after_streams_finish) { _active = 0; return; }
        linger_fin += ms_since_last_tick;
        if (linger_fin >= 10 * _cfg.rt_timeout) _active = 0;
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window(), forward(_sender.segments_out().size());
}

void TCPConnection::connect() {
    _sender.fill_window();
    _segments_out.push(_sender.segments_out().front());
    _sender.segments_out().pop();
}

void TCPConnection::forward(int sz) {
    while (sz--) {
        TCPSegment tmp = _sender.segments_out().front(); _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) tmp.header().ackno = _receiver.ackno().value(), tmp.header().ack = 1;
        tmp.header().win = _receiver.window_size();
        if (tmp.header().fin) fin_sent = 1;
        _segments_out.push(tmp);
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) terminate(1);
    } catch (...) {}
}
