#include "tcp_sender.hh"

#include "tcp_config.hh"

TCPSegment TCPSender::make_seg(bool syn_, bool fin, bool rst, WrappingInt32 seq_no, WrappingInt32 ack_no, 
                               bool pld, Buffer bf) {
    TCPSegment tmp;
    tmp.header().syn = syn_;
    tmp.header().fin = fin;
    tmp.header().rst = rst;
    tmp.header().seqno = seq_no;
    tmp.header().ackno = ack_no;
    if (pld) tmp.payload() = bf;
    return tmp;
}

void TCPSender::fill_window() {
    if (!syn) {
        syn = 1;
        _next_seqno = unack = 1;
        seg = make_seg(1, 0, 0, _isn, next_seqno());
        _segments_out.push(seg);
        chq[next_seq()] = seg;
    } else if (stream_in().eof() && !_eof && (prev_mx >= next_seq() || (unack && wind > unack))) {
        seg = make_seg(0, 1, 0, next_seqno(), wrap(1, next_seqno()));
        ++_next_seqno, ++unack;
        _segments_out.push(chq[next_seq()] = seg);
        _eof = 1;
    } else if (!_eof && prev_mx >= _isn.raw_value()) {
        while (!_eof && wind && stream_in().buffer_size()) {
            wind -= (len = min(stream_in().buffer_size(), min(wind, TCPConfig::MAX_PAYLOAD_SIZE)));
            cur = stream_in().read(len);
            if (stream_in().input_ended() && wind) {
                _eof = 1;
                seg = make_seg(0, 1, 0, next_seqno(), wrap(1, next_seqno()), 1, Buffer(move(cur)));
                ++_next_seqno, ++unack;
            } else seg = make_seg(0, 0, 0, next_seqno(), wrap(len, next_seqno()), 1, Buffer(move(cur)));
            _next_seqno += len, unack += len;
            _segments_out.push(chq[next_seq()] = seg);
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    if ((Ack = ackno.raw_value()) > next_seq()) return;
    if (!window_size) wind = 1, backoff = 0;
    else wind = window_size;
    if (_eof == 1 && Ack == next_seq()) _eof = 2;
    if (Ack > prev_mx) {
        if (chq.size()) rto = _initial_retransmission_timeout, tot_ms = 0;
        conseq = 0;
    }
    prev_mx = max(Ack, prev_mx);
    if (ackno == _isn) return;
    for (auto iter = chq.begin(), ed = chq.end(); iter != ed && iter->first <= Ack;) {
        unack -= chq[iter->first].length_in_sequence_space();
        chq.erase(iter++);
    }
    if (wind && (stream_in().buffer_size() + unack)) {fill_window();}
}

// ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (chq.size() && (tot_ms += ms_since_last_tick) >= rto) {
        tot_ms = 0;
        resend = 1;
        if (backoff) {
            rto <<= 1, ++conseq;
            if (conseq > TCPConfig::MAX_RETX_ATTEMPTS) return;
        }
        _segments_out.push(chq.begin()->second);
   } else resend = 0;
}
