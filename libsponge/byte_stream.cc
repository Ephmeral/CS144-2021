#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : buffer(capacity), cap(capacity)  {
    buffer.erase(buffer.begin(), buffer.end());
}

size_t ByteStream::write(const string &data) {
    if (input_end) {
      return 0;
    }
    size_t pre = buffer.size();
    for (char c : data) {
        if (buffer.size() < this->cap) {
            buffer.push_back(c);
            this->write_len++;
        } else {
            break;
        }
    }
    return buffer.size() - pre;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string ans;
    size_t i = 0;
    for (auto it = buffer.begin(); it != buffer.end() && i < len;
         ++it, ++i) {
      ans.push_back(*it);
    }
    return ans;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    for (size_t i = 0; i < len && !buffer.empty(); i++) {
        buffer.pop_front();
        this->read_len++;
    }
 }

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string ans;
    size_t i = 0;
    for (auto it = buffer.begin();
         it != buffer.end() && i < len && !buffer.empty(); ++it, ++i) {
      ans.push_back(*it);
      buffer.pop_front();
      this->read_len++;
    }
    return ans;
}

void ByteStream::end_input() { input_end = true; }

bool ByteStream::input_ended() const { return input_end; }

size_t ByteStream::buffer_size() const { return buffer.size(); }

bool ByteStream::buffer_empty() const { return buffer.empty(); }

bool ByteStream::eof() const { return buffer.empty() && this->input_end; }

size_t ByteStream::bytes_written() const { return this->write_len; }

size_t ByteStream::bytes_read() const { return this->read_len; }

size_t ByteStream::remaining_capacity() const { return this->cap - buffer.size(); }
