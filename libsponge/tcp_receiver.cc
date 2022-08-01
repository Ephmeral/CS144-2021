#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();
    bool eof = header.fin;
    if (_is_isn == false && header.syn == false) {
        return;
    }
    WrappingInt32 seqno = header.seqno;
    if (header.syn) {
        _is_isn = true;
        _isn = header.seqno;
        seqno = seqno + 1;
    }

    uint64_t lastIndex = stream_out().bytes_written();
    uint64_t index = unwrap(seqno, _isn, lastIndex) - 1;
    _reassembler.push_substring(seg.payload().copy(), index, eof);  // eof is true
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (_is_isn) {
      size_t ack = stream_out().bytes_written() + 1;
      if (stream_out().input_ended()) return wrap(ack + 1, _isn);
      return wrap(ack, _isn);
    }
    return {}; 
}

size_t TCPReceiver::window_size() const { return stream_out().remaining_capacity(); }
