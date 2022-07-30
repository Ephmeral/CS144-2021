#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : 
    _bufstr(capacity, '\0'), 
    _flag(capacity, false),
    _firstid(0), 
    _output(capacity), 
    _capacity(capacity),
    _eof(0),
    _unassembled_len(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t start = max(index, _firstid);
    size_t end = min(index + data.length(), _firstid + _output.remaining_capacity());

    for (size_t i = start; i < end; i++) {
        size_t real_id = i % _capacity;
        if (_flag[real_id]) continue;
        _bufstr[real_id] = data[i - index];
        _flag[real_id] = true;
        ++_unassembled_len;
    }

    size_t real_end = _firstid;
    while (_flag[real_end % _capacity] == true && real_end < _firstid + _capacity) {
        ++real_end;
    }

    if (real_end > _firstid) {
        std::string write_str(real_end - _firstid, 0);
        for (size_t i = _firstid; i < real_end; ++i) {
            write_str[i - _firstid] = _bufstr[i % _capacity];
        }

        size_t n = _output.write(write_str);
        for (size_t i = _firstid; i < _firstid + n; ++i) {
            _flag[i % _capacity] = false;
        }

        _firstid += n;
        _unassembled_len -= n;
    }
    
    if (eof) {
        _eof = index + data.size() + 1; 
    }
    if (_output.bytes_written() + 1 == _eof) { 
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { 
    return _unassembled_len;
}

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }
