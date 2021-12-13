#include "byte_stream.hh"

ByteStream::ByteStream(const size_t capacity) : buf(), cap(capacity) {}

size_t ByteStream::write(const string &data) {
    size_t n = min(data.size(), cap - buf.size());
    for (size_t i = 0; i < n; ++i) buf.push_back(data[i]);
    wc += n;
    return n;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string res(buf.begin(), buf.begin() + len);
    return res;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    for (size_t i = 0; i < len && !buf.empty(); ++i, ++rc) buf.pop_front();
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string res = peek_output(len); pop_output(len);
    return res;
}

void ByteStream::end_input() {
    _eoi = 1;
}

bool ByteStream::input_ended() const {return _eoi;}

size_t ByteStream::buffer_size() const { return buf.size(); }

bool ByteStream::buffer_empty() const { return !buf.size(); }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return wc; }

size_t ByteStream::bytes_read() const { return rc; }

size_t ByteStream::remaining_capacity() const { return cap - buf.size(); }
