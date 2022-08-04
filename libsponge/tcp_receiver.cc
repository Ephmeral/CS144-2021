#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
	//得到TCP报文段的头部
    TCPHeader header = seg.header();
    bool eof = header.fin; //eof标记根据FIN标志来决定
    //在发起新的连接之前，所有报文段都丢弃
    if (_is_syn == false && header.syn == false) {
        return;
    }
    //得到序列号
    WrappingInt32 seqno = header.seqno;
    //如果SYN标记位为真，表示发起一个新的连接
    //此时设置对应的变量，序列号也要+1
    //序列号加1的原因是syn标记位也占了一位，但是不会写入流当中
    if (header.syn) {
        _is_syn = true;
        _isn = header.seqno;
        seqno = seqno + 1;
    }
	
	//获取当前已经写入流当中的字节数，用来推导seqno的下标
    uint64_t lastIndex = stream_out().bytes_written();
    //利用前面实现的unwrap得到待写入数据的起始下标
    uint64_t index = unwrap(seqno, _isn, lastIndex) - 1;
    //将数据部分写入流当中
    _reassembler.push_substring(seg.payload().copy(), index, eof); 
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (_is_syn) {
      size_t ack = stream_out().bytes_written() + 1;
      if (stream_out().input_ended()) ack++;
      return wrap(ack, _isn);
    }
    return {}; 
}

size_t TCPReceiver::window_size() const { return stream_out().remaining_capacity(); }
