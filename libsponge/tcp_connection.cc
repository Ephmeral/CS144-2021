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
    // 处于未连接状态直接返回
    if (!_is_active) {
        return;
    }

    // 接收的报文段中设置了RST标志，直接设置错误并关闭TCP连接
    if (seg.header().rst) {
        send_rst_segment(false);
        return;
    }

    // 重置收到TCP报文段的时间
    _time_since_last_segment_received = 0;

    // 没有收到RST标志，交给 receiver 进行处理
    _receiver.segment_received(seg);

    //
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED) {
        connect();
        return;
    }

    // 是否需要发送一个空的TCP数据报，用来维持 keep-alive 状态
    // 接收的报文段如果有内容的话，考虑可能是需要维持 keep-alive 状态，设为true
    bool send_empty = seg.length_in_sequence_space() > 0;

    // 如果接受的报文段有 ackno(序列号)，设置 sender 的确认号和窗口大小，相当于 sender 接收到一个新的确认
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    if (send_empty) {
        // 如果 TCPReceiver 有确认号，但是 TCPSender 没有数据，说明上面接收到的确认是一个乱序/不合理的序列号
        // 此时发送一个空的数据报，用来维持 keep-alive 状态
        if (_receiver.ackno().has_value() && _sender.segments_out().empty()) {
            _sender.send_empty_segment();
        }
    }

    // 尽可能的将 TCPSender 中的数据发送出去
    send_segments(false);
    // 如果有需要则静默的关闭连接
    clean_shutdown();
}

bool TCPConnection::active() const { return _is_active; }

size_t TCPConnection::write(const string &data) {
    // 将数据写入 TCPSender 中，并返回实际写入数据的大小
    size_t write_size = _sender.stream_in().write(data);
    // 尽可能的将 TCPSender 中的数据发送出去
    send_segments(true);
    return write_size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    // 更新 TCPSender 的tick
    _sender.tick(ms_since_last_tick);
    // 连续重传次数超过上限，发送 RST 数据包，断开连接
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        _sender.segments_out().pop();
        send_rst_segment(true);
        return;
    }
    // 更新上一次接收数据的时间
    _time_since_last_segment_received += ms_since_last_tick;
    // 尽可能的将 TCPSender 中的数据发送出去
    send_segments(false);
    // 如果有需要则静默的关闭连接
    clean_shutdown();
}

void TCPConnection::end_input_stream() {
    // 输入流结束，关闭TCPSender输出流，并且尽可能填充TCPSender，然后发送数据
    _sender.stream_in().end_input();
    send_segments(true);
}

void TCPConnection::connect() {
    // 建立连接，并且尽可能填充TCPSender，然后发送数据
    _is_active = true;
    send_segments(true);
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            send_rst_segment(false);
            // Your code here: need to send an RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_rst_segment(bool is_rst) {
    // 如果需要发送RST数据报
    if (is_rst) {
        TCPSegment seg;
        seg.header().rst = true;
        _segments_out.push(seg);
    }
    // TCPReceiver 和 TCPSender 的流设置为错误模式
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    // 关闭TCP连接
    _linger_after_streams_finish = false;
    _is_active = false;
}

void TCPConnection::send_segments(bool is_fill_window) {
    // 需要 TCPSender 填充窗口的情况
    if (is_fill_window)
        _sender.fill_window();
    // 将 TCPSender 中的数据都发送出去
    while (!_sender.segments_out().empty()) {
        // 从队列中取出数据
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        // 如果 TCPReceiver 有确认号，设置将每个需要发送的数据报的确认号和窗口大小
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        // 将数据报发送出去
        _segments_out.push(seg);
    }
    clean_shutdown();
}

void TCPConnection::clean_shutdown() {
    // CLOSE_WAIT 此时服务器等待连接关闭
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED) {
        _linger_after_streams_finish = false;
    }

    // 准备断开连接
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED) {
        // 如果处于 TIME_WAIT 并且超时 或者处于 CLOSE_WAIT 状态，关闭TCP连接
        if (!_linger_after_streams_finish || _time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
            _is_active = false;
        }
    }
}
