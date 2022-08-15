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
    //窗口大小为0的情况时，按照大小为1进行处理
    size_t wsize = _window_size == 0 ? 1 : _window_size;
    //尽可能的填满窗口
    while (wsize > _outstanding_bytes) {
        TCPSegment segment;
        //如果没有设置syn，则设置此时TCP段为syn
        if (!_is_syn) {
            _is_syn = segment.header().syn = true;
        }
        //设置TCP段的序列号
        segment.header().seqno = next_seqno();

        //TCP段中payload最大可以填入的数据（注意syn标志占1位）
        const size_t payload_size =
            min(TCPConfig::MAX_PAYLOAD_SIZE, wsize - _outstanding_bytes - segment.header().syn);
        //从_stream中读取数据
        string payload = _stream.read(payload_size);

        //如果没有设置fin，并且_stream已经读取完，并且窗口大小还能继续填充数据
        //设置fin标志位
        if (!_is_fin && _stream.eof() && payload.size() + _outstanding_bytes < wsize)
            _is_fin = segment.header().fin = true;
        //数据写入TCP段中
        segment.payload() = Buffer(std::move(payload));

        // 如果没有任何数据，则停止数据包的发送
        if (segment.length_in_sequence_space() == 0)
            break;

        // 如果没有正在等待的数据包，则重设更新时间
        if (_unrcv_queue.empty()) {
            _rto = _initial_retransmission_timeout;
            _timer = 0;
        }

        // 发送TCP段
        _segments_out.push(segment);

        // 追踪这些TCP段
        _outstanding_bytes += segment.length_in_sequence_space();
        _unrcv_queue.push(segment);
        // 更新待发送 abs seqno
        _next_seqno += segment.length_in_sequence_space();

        // 如果设置了 fin，则直接退出填充窗口的操作
        if (segment.header().fin)
            break;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    //得到接收方发送过来的确认号对应的实际序列号
    uint64_t abs_seqno = unwrap(ackno, _isn, _next_seqno);
    //采用累计确认，超过的段直接丢弃
    if (abs_seqno > _next_seqno) return;
    //枚举未确认的段
    while (!_unrcv_queue.empty()) {
        TCPSegment segment = _unrcv_queue.front();
        //未确认的段的实际序列号超过abs_seqno，表示后续的段都未接收
        if (unwrap(segment.header().seqno, _isn, _next_seqno) 
                + segment.length_in_sequence_space() > abs_seqno) break;
        //对于abs_seqno之前的数据段，说明未确认的段现在被接收了，从队列中删除
        _outstanding_bytes -= segment.length_in_sequence_space();
        _unrcv_queue.pop();
        //设置超时重传时间
        _rto = _initial_retransmission_timeout;
        _timer = 0;
    }
    //连续重传次数清空
    _consecutive_retransmissions_count = 0;
    //设置窗口大小，并且尽可能填充数据
    _window_size = window_size;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    //设置重传计时器的时间
    _timer += ms_since_last_tick; 
    //如果还有已发送未确认的段，并且超时
    if (!_unrcv_queue.empty() && _timer >= _rto) {
        //如果窗口大小不为0，超时重传时间*2
        if (_window_size > 0) _rto *= 2;
        //重置重传计时器
        _timer = 0;
        //超时情况重新发送数据
        _segments_out.push(_unrcv_queue.front());
        //连续重传次数加1
        ++_consecutive_retransmissions_count;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions_count; }

void TCPSender::send_empty_segment() {
    //创建一个TCP段
    TCPSegment segment;
    //设置TCP头部的seqence number
    segment.header().seqno = next_seqno();
    //发送TCP段
    _segments_out.push(segment);
}
