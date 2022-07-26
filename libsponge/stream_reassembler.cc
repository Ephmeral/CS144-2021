#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : 
    _maps(), 
    _endid(0), 
    _output(capacity), 
    _capacity(capacity),
    eof_index(0),
    _unassembled_len(0) {}

void StreamReassembler::merge() {
    size_t real_id, real_len;
    for (auto it = _maps.begin(); it != _maps.end(); ) {
        size_t index = it->first;
        string data = it->second;
        // if (index > _endid) break;
        if (index <= this->_endid && index + data.length() >= this->_endid) {
            real_id = _endid - index;
            // 防止超过cap的情况
            real_len = min(data.length() - real_id, _capacity - _endid);
            _output.write(data.substr(real_id, real_len));
            _unassembled_len -= real_len;
            _endid += real_len;
            ++it;
            _maps.erase(index);
        } else if (index < this->_endid && index + data.length() < this->_endid) {
            ++it;
            _maps.erase(index);
        } else {
            ++it;
        }
    }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // size_t real_id, real_len;
    if (index < this->_endid && index + data.length() < this->_endid) {
        return;
    }
    if (!_maps.count(index) || _maps[index].length() < data.length()) {
        _maps[index] = data;
    } 
    
    _unassembled_len += data.length();
    merge();

    if (eof) {
        eof_index = index + data.size() + 1; // 注意，这里的+1很重要
    }
    if (_output.bytes_written() + 1 == eof_index) { // 和上面的+1对应上
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { 
    size_t total = 0;
    size_t preid = 0, prelen = 0;
    for (auto it = _maps.begin(); it != _maps.end(); ++it) {
        size_t curid = it->first, curlen = it->second.length();
        if (curid < preid + prelen) {
            total -= preid + prelen - curid; //减掉重叠部分
        }
        total += curlen;
        preid = curid;
        prelen = curlen;
    }
    // return total;
    return total;
}

bool StreamReassembler::empty() const { return _maps.empty() && _output.buffer_empty(); }
