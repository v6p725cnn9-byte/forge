#include "engine/net/transport/stream.hpp"

#include <cmath>
#include <cstring>

namespace forge::net {

void ByteWriter::u8(std::uint8_t v) { bytes_.push_back(v); }

void ByteWriter::u16(std::uint16_t v)
{
    bytes_.push_back(static_cast<std::uint8_t>(v));
    bytes_.push_back(static_cast<std::uint8_t>(v >> 8));
}

void ByteWriter::u32(std::uint32_t v)
{
    bytes_.push_back(static_cast<std::uint8_t>(v));
    bytes_.push_back(static_cast<std::uint8_t>(v >> 8));
    bytes_.push_back(static_cast<std::uint8_t>(v >> 16));
    bytes_.push_back(static_cast<std::uint8_t>(v >> 24));
}

void ByteWriter::f32(float v)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &v, 4);
    u32(bits);
}

void ByteWriter::bytes(const void* data, std::size_t size)
{
    const auto* p = static_cast<const std::uint8_t*>(data);
    bytes_.insert(bytes_.end(), p, p + size);
}

void ByteWriter::pad(const char* text, std::size_t width)
{
    const std::size_t n = text ? std::strlen(text) : 0;
    for (std::size_t i = 0; i < width; ++i) u8(i < n ? static_cast<std::uint8_t>(text[i]) : 0);
}

void ByteWriter::poke_u8(std::size_t offset, std::uint8_t v)
{
    if (offset >= bytes_.size()) return;
    bytes_[offset] = v;
}

void ByteWriter::poke_u16(std::size_t offset, std::uint16_t v)
{
    if (offset + 1 >= bytes_.size()) return;
    bytes_[offset] = static_cast<std::uint8_t>(v);
    bytes_[offset + 1] = static_cast<std::uint8_t>(v >> 8);
}

std::vector<std::uint8_t> ByteWriter::take() { return std::move(bytes_); }

ByteReader::ByteReader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}

bool ByteReader::need(std::size_t n) const { return data_ && offset_ <= size_ && n <= size_ - offset_; }

bool ByteReader::u8(std::uint8_t& v)
{
    if (!need(1)) return false;
    v = data_[offset_++];
    return true;
}

bool ByteReader::u16(std::uint16_t& v)
{
    std::uint8_t a = 0, b = 0;
    if (!u8(a) || !u8(b)) return false;
    v = static_cast<std::uint16_t>(a | (static_cast<std::uint16_t>(b) << 8));
    return true;
}

bool ByteReader::u32(std::uint32_t& v)
{
    std::uint16_t lo = 0, hi = 0;
    if (!u16(lo) || !u16(hi)) return false;
    v = static_cast<std::uint32_t>(lo) | (static_cast<std::uint32_t>(hi) << 16);
    return true;
}

bool ByteReader::f32(float& v)
{
    std::uint32_t bits = 0;
    if (!u32(bits)) return false;
    std::memcpy(&v, &bits, 4);
    return std::isfinite(v);
}

bool ByteReader::pad(std::string& text, std::size_t width)
{
    if (!need(width)) return false;
    text.assign(reinterpret_cast<const char*>(data_ + offset_), width);
    const auto zero = text.find('\0');
    if (zero != std::string::npos) text.resize(zero);
    offset_ += width;
    return true;
}

bool ByteReader::skip(std::size_t n)
{
    if (!need(n)) return false;
    offset_ += n;
    return true;
}

} // namespace forge::net
