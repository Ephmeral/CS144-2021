#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : 
    _bufstr(capacity, ' '),
    _flag(capacity, false),
    _output(capacity), 
    _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    //得到字符读取的开始和结束的位置
    size_t start = max(index, _firstid);
    //注意这里用的是_output剩余的大小，而不是_capacity
    size_t end = min(index + data.length(), _firstid + _output.remaining_capacity());

    //从每个位置开始遍历，将对应字符写入缓存bufstr中
    for (size_t i = start; i < end; i++) {
        //这里是实际写入bufstr的位置，形成一个环
        size_t real_id = i % _capacity;
        if (_flag[real_id]) continue;
        _bufstr[real_id] = data[i - index];
        _flag[real_id] = true;
        ++_unassembled_len;
    }

    //找到真正写入的字符串结尾位置
    size_t real_end = _firstid;
    while (_flag[real_end % _capacity] && real_end < _firstid + _capacity) {
        ++real_end;
    }

    //如果字符串能够写入output中
    if (real_end > _firstid) {
        //从bufstr中得到写入的字符串
        std::string write_str(real_end - _firstid, 0);
        for (size_t i = _firstid; i < real_end; ++i) {
            write_str[i - _firstid] = _bufstr[i % _capacity];
        }

        //实际写入后得到的大小（可能有超过capcity的情况
        size_t n = _output.write(write_str);
        for (size_t i = _firstid; i < _firstid + n; ++i) {
            _flag[i % _capacity] = false;
        }

        //第一次未读入的位置往后偏移 n
        _firstid += n;
        //未组装的长度 - n
        _unassembled_len -= n;
    }
    //所有的读取结束时
    if (eof) {
        _eof = index + data.size() + 1; // 注意，这里的+1很重要
    }
    if (_output.bytes_written() + 1 == _eof) { // 和上面的+1对应上
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { 
    return _unassembled_len;
}

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }
