#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace forge::net {

class ByteWriter {
public:
    void u8(std::uint8_t v);
    void u16(std::uint16_t v);
    void u32(std::uint32_t v);
    void f32(float v);
    void bytes(const void* data, std::size_t size);
    void pad(const char* text, std::size_t width);
    void poke_u8(std::size_t offset, std::uint8_t v);
    void poke_u16(std::size_t offset, std::uint16_t v);
    std::size_t size() const { return bytes_.size(); }
    const std::uint8_t* data() const { return bytes_.data(); }
    const std::vector<std::uint8_t>& buffer() const { return bytes_; }
    std::vector<std::uint8_t> take();

private:
    std::vector<std::uint8_t> bytes_;
};

class ByteReader {
public:
    ByteReader() = default;
    ByteReader(const std::uint8_t* data, std::size_t size);

    bool need(std::size_t n) const;
    bool u8(std::uint8_t& v);
    bool u16(std::uint16_t& v);
    bool u32(std::uint32_t& v);
    bool f32(float& v);
    bool pad(std::string& text, std::size_t width);
    bool skip(std::size_t n);
    std::size_t offset() const { return offset_; }
    std::size_t size() const { return size_; }
    const std::uint8_t* data() const { return data_; }
    bool done() const { return offset_ == size_; }

private:
    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t offset_ = 0;
};

} // namespace forge::net
