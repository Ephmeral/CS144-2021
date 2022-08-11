#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _outstanding_bytes; }

void TCPSender::fill_window() { 
    size_t wsize = _window_size == 0 ? 1 : _window_size;
    while (wsize > _outstanding_bytes) {
        TCPSegment segment;
        if (!_is_syn) {
            _is_syn = segment.header().syn = true;
        }
        segment.header().seqno = next_seqno();

        const size_t payload_size =
            min(TCPConfig::MAX_PAYLOAD_SIZE, wsize - _outstanding_bytes - segment.header().syn);
        string payload = _stream.read(payload_size);

        if (!_is_fin && _stream.eof() && payload.size() + _outstanding_bytes < wsize)
            _is_fin = segment.header().fin = true;

        segment.payload() = Buffer(move(payload));

        // 如果没有任何数据，则停止数据包的发送
        if (segment.length_in_sequence_space() == 0)
            break;

        // 如果没有正在等待的数据包，则重设更新时间
        if (_unrcv_queue.empty()) {
            _timeout = _initial_retransmission_timeout;
            _time_count = 0;
        }

        // 发送
        _segments_out.push(segment);

        // 追踪这些数据包
        _outstanding_bytes += segment.length_in_sequence_space();
        _unrcv_queue.push(segment);
        // 更新待发送 abs seqno
        _next_seqno += segment.length_in_sequence_space();

        // 如果设置了 fin，则直接退出填充 window 的操作
        if (segment.header().fin)
            break;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    // DUMMY_CODE(ackno, window_size);
    uint64_t abs_seqno = unwrap(ackno, _isn, _next_seqno);
    //超过的段直接丢弃
    if (abs_seqno > _next_seqno) return;
    while (!_unrcv_queue.empty()) {
        TCPSegment segment = _unrcv_queue.front();
        if (unwrap(segment.header().ackno, _isn, _next_seqno) 
                + segment.length_in_sequence_space() > abs_seqno) break;
        _outstanding_bytes -= segment.length_in_sequence_space();
        _unrcv_queue.pop();
        _timeout = _initial_retransmission_timeout;
        _time_count = 0;
    }
    _consecutive_retransmissions_count = 0;
    _window_size = window_size;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _time_count += ms_since_last_tick;
    if (!_unrcv_queue.empty() && _time_count >= _timeout) {
        if (_window_size > 0) _timeout *= 2;
        _time_count = 0;
        _segments_out.push(_unrcv_queue.front());
        ++_consecutive_retransmissions_count;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions_count; }

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    _segments_out.push(segment);
}
