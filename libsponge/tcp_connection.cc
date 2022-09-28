#include "tcp_connection.hh"

#include <cassert>
#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    _time_since_last_segment_received = 0;
    // 如果发来的是一个 ACK 包，则无需发送 ACK
    bool need_send_ack = seg.length_in_sequence_space() > 0;

    // 读取并处理接收到的数据
    // _receiver 足够鲁棒以至于无需进行任何过滤
    _receiver.segment_received(seg);

    // 如果是 RST 包，则直接终止
    //! NOTE: 当 TCP 处于任何状态时，均需绝对接受 RST。因为这可以防止尚未到来数据包产生的影响
    if (seg.header().rst) {
        send_rst_segment(false);
        return;
    }

    // 如果收到了 ACK 包，则更新 _sender 的状态并补充发送数据
    // NOTE: _sender 足够鲁棒以至于无需关注传入 ack 是否可靠
    assert(_sender.segments_out().empty());
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        // _sender.fill_window(); // 这行其实是多余的，因为已经在 ack_received 中被调用了，不过这里显示说明一下其操作
        // 如果原本需要发送空ack，并且此时 sender 发送了新数据，则停止发送空ack
        if (need_send_ack && !_sender.segments_out().empty())
            need_send_ack = false;
    }

    // 如果是 LISEN 到了 SYN
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED) {
        // 此时肯定是第一次调用 fill_window，因此会发送 SYN + ACK
        connect();
        return;
    }

    // 判断 TCP 断开连接时是否时需要等待
    // CLOSE_WAIT
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED)
        _linger_after_streams_finish = false;

    // 如果到了准备断开连接的时候。服务器端先断
    // CLOSED
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && !_linger_after_streams_finish) {
        _is_active = false;
        return;
    }

    // 如果收到的数据包里没有任何数据，则这个数据包可能只是为了 keep-alive
    if (need_send_ack)
        _sender.send_empty_segment();
    send_segments(false);
}

bool TCPConnection::active() const { return _is_active; }

size_t TCPConnection::write(const string &data) {
    size_t wsize = _sender.stream_in().write(data);
    send_segments(true);
    return wsize;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    assert(_sender.segments_out().empty());
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        // 在发送 rst 之前，需要清空可能重新发送的数据包
        _sender.segments_out().pop();
        send_rst_segment(true);
        return;
    }
    // 转发可能重新发送的数据包
    send_segments(false);

    _time_since_last_segment_received += ms_since_last_tick;

    // 如果处于 TIME_WAIT 状态并且超时，则可以静默关闭连接
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && _linger_after_streams_finish &&
        _time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
        _is_active = false;
        _linger_after_streams_finish = false;
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    send_segments(true);
}

void TCPConnection::connect() {
    _is_active = true;
    send_segments(true);
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            send_rst_segment(true);
            // Your code here: need to send an RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_rst_segment(bool set_rst) {
    if (set_rst) {
        TCPSegment rst_segment;
        rst_segment.header().rst = true;
        _segments_out.push(rst_segment);
    }
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _linger_after_streams_finish = false;
    _is_active = false;
}

void TCPConnection::send_segments(bool isFillWindow) {
    if (isFillWindow) _sender.fill_window();
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(seg);
    }
}
