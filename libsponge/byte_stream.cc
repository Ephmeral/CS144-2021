#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : cap(capacity)  {}

size_t ByteStream::write(const string &data) {
    if (input_end) {
      return 0;
    }
    size_t len = min(data.length(), cap - buffer.size());
    write_len += len;
    buffer.append(BufferList(std::move(string().assign(data.begin(),data.begin()+len))));
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t length = len;
    if (length > buffer.size()) {
        length = buffer.size();
    }
    string s= buffer.concatenate();
    return string().assign(s.begin(), s.begin() + length);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    size_t length = len;
    if (length > buffer.size()) {
        length = buffer.size();
    }
    read_len += length;
    buffer.remove_prefix(length);
 }

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string ans;
//    size_t i = 0;
    size_t length = len;
    if (length > buffer.size()) {
        length = buffer.size();
    }
    string s = buffer.concatenate();
    buffer.remove_prefix(length);
    read_len += length;
//    for (auto it = buffer.begin();
//         it != buffer.end() && i < len && !buffer.empty(); ++it, ++i) {
//      ans.push_back(*it);
//      buffer.pop_front();
//      this->read_len++;
//    }
    return string().assign(s.begin(), s.begin() + length);
}

void ByteStream::end_input() { input_end = true; }

bool ByteStream::input_ended() const { return input_end; }

size_t ByteStream::buffer_size() const { return buffer.size(); }

bool ByteStream::buffer_empty() const { return buffer.size() == 0; }

bool ByteStream::eof() const { return buffer.size() == 0 && this->input_end; }

size_t ByteStream::bytes_written() const { return this->write_len; }

size_t ByteStream::bytes_read() const { return this->read_len; }

size_t ByteStream::remaining_capacity() const { return this->cap - buffer.size(); }
