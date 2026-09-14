#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace forge::core {

// Bump allocator for short-lived frame/scratch data. Reset each frame.
class Arena {
public:
    explicit Arena(std::size_t bytes = 64 * 1024) { storage_.resize(bytes); }

    void* allocate(std::size_t bytes, std::size_t align = alignof(std::max_align_t))
    {
        const std::size_t mask = align - 1;
        const std::size_t aligned = (offset_ + mask) & ~mask;
        if (aligned + bytes > storage_.size()) return nullptr;
        offset_ = aligned + bytes;
        return storage_.data() + aligned;
    }

    void reset() { offset_ = 0; }
    std::size_t used() const { return offset_; }
    std::size_t capacity() const { return storage_.size(); }

private:
    std::vector<std::uint8_t> storage_;
    std::size_t offset_ = 0;
};

} // namespace forge::core
