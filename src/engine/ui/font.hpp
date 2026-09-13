#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace forge::ui {

struct Glyph {
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
    float advance = 0;
};

std::uint32_t utf8_next(const char*& cursor, const char* end);

class Font {
public:
    bool bake(const std::filesystem::path& ttf, float pixel_height);
    const Glyph* glyph(std::uint32_t codepoint) const;
    float width(std::string_view text, float scale = 1.0f) const;
    float size() const { return size_; }
    float ascent() const { return ascent_; }
    int atlas_width() const { return atlas_w_; }
    int atlas_height() const { return atlas_h_; }
    const std::uint8_t* pixels() const { return pixels_.data(); }
    float white_u() const { return white_u_; }
    float white_v() const { return white_v_; }

private:
    std::unordered_map<std::uint32_t, Glyph> glyphs_;
    std::vector<std::uint8_t> pixels_;
    int atlas_w_ = 0;
    int atlas_h_ = 0;
    float size_ = 0;
    float ascent_ = 0;
    float white_u_ = 0;
    float white_v_ = 0;
};

} // namespace forge::ui
