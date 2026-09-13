#include "engine/ui/font.hpp"

#include <stb_rect_pack.h>
#include <stb_truetype.h>
#include <cmath>
#include <fstream>
#include <iterator>

namespace forge::ui {

std::uint32_t utf8_next(const char*& cursor, const char* end)
{
    if (cursor >= end) return 0;
    const auto lead = static_cast<unsigned char>(*cursor++);
    if (lead < 0x80) return lead;
    int extra = 0;
    std::uint32_t cp = 0;
    if ((lead & 0xE0) == 0xC0) {
        extra = 1;
        cp = lead & 0x1F;
    } else if ((lead & 0xF0) == 0xE0) {
        extra = 2;
        cp = lead & 0x0F;
    } else if ((lead & 0xF8) == 0xF0) {
        extra = 3;
        cp = lead & 0x07;
    } else {
        return 0xFFFD;
    }
    for (int i = 0; i < extra; ++i) {
        if (cursor >= end) return 0xFFFD;
        const auto cont = static_cast<unsigned char>(*cursor++);
        if ((cont & 0xC0) != 0x80) return 0xFFFD;
        cp = (cp << 6) | (cont & 0x3F);
    }
    return cp;
}

bool Font::bake(const std::filesystem::path& ttf, float pixel_height)
{
    std::ifstream in(ttf, std::ios::binary);
    if (!in) return false;
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.empty()) return false;

    atlas_w_ = 1024;
    atlas_h_ = 1024;
    pixels_.assign(static_cast<std::size_t>(atlas_w_ * atlas_h_), 0);
    size_ = pixel_height;

    stbtt_fontinfo info{};
    if (!stbtt_InitFont(&info, bytes.data(), 0)) return false;
    const float scale = stbtt_ScaleForPixelHeight(&info, pixel_height);
    int raw_ascent = 0;
    stbtt_GetFontVMetrics(&info, &raw_ascent, nullptr, nullptr);
    ascent_ = static_cast<float>(raw_ascent) * scale;

    stbtt_pack_context pack{};
    if (!stbtt_PackBegin(&pack, pixels_.data(), atlas_w_, atlas_h_, 0, 1, nullptr)) return false;
    stbtt_PackSetOversampling(&pack, 2, 2);

    stbtt_pack_range ranges[2]{};
    stbtt_packedchar latin[96]{};
    stbtt_packedchar cyrillic[96]{};
    ranges[0].font_size = pixel_height;
    ranges[0].first_unicode_codepoint_in_range = 32;
    ranges[0].num_chars = 96;
    ranges[0].chardata_for_range = latin;
    ranges[1].font_size = pixel_height;
    ranges[1].first_unicode_codepoint_in_range = 0x400;
    ranges[1].num_chars = 96;
    ranges[1].chardata_for_range = cyrillic;
    const bool packed = stbtt_PackFontRanges(&pack, bytes.data(), 0, ranges, 2) != 0;
    stbtt_PackEnd(&pack);
    if (!packed) return false;

    const auto store = [this](int first, int count, const stbtt_packedchar* data) {
        for (int i = 0; i < count; ++i) {
            const auto& c = data[i];
            if (c.x1 <= c.x0 || c.y1 <= c.y0) continue;
            Glyph g;
            g.x0 = c.xoff;
            g.y0 = c.yoff;
            g.x1 = c.xoff2;
            g.y1 = c.yoff2;
            g.u0 = c.x0 / static_cast<float>(atlas_w_);
            g.v0 = c.y0 / static_cast<float>(atlas_h_);
            g.u1 = c.x1 / static_cast<float>(atlas_w_);
            g.v1 = c.y1 / static_cast<float>(atlas_h_);
            g.advance = c.xadvance;
            glyphs_[static_cast<std::uint32_t>(first + i)] = g;
        }
    };
    store(32, 96, latin);
    store(0x400, 96, cyrillic);

    const int wx = atlas_w_ - 2;
    const int wy = atlas_h_ - 2;
    pixels_[static_cast<std::size_t>(wy * atlas_w_ + wx)] = 255;
    pixels_[static_cast<std::size_t>(wy * atlas_w_ + wx + 1)] = 255;
    pixels_[static_cast<std::size_t>((wy + 1) * atlas_w_ + wx)] = 255;
    pixels_[static_cast<std::size_t>((wy + 1) * atlas_w_ + wx + 1)] = 255;
    white_u_ = (static_cast<float>(wx) + 0.5f) / static_cast<float>(atlas_w_);
    white_v_ = (static_cast<float>(wy) + 0.5f) / static_cast<float>(atlas_h_);
    return glyphs_.size() > 32;
}

const Glyph* Font::glyph(std::uint32_t codepoint) const
{
    const auto it = glyphs_.find(codepoint);
    if (it == glyphs_.end()) return nullptr;
    return &it->second;
}

float Font::width(std::string_view text, float scale) const
{
    float x = 0;
    const char* p = text.data();
    const char* end = p + text.size();
    while (p < end) {
        const auto cp = utf8_next(p, end);
        if (cp == 0) break;
        const auto* g = glyph(cp);
        x += (g ? g->advance : size_ * 0.5f) * scale;
    }
    return x;
}

} // namespace forge::ui
