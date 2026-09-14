#include "engine/ui/font.hpp"

#include <stb_rect_pack.h>
#include <stb_truetype.h>
#include <cmath>
#include <fstream>
#include <iterator>

namespace forge::ui {
namespace {

// SDF is baked above the requested size so UI text stays crisp when scaled up.
// Padding is in bake-scale texels; the distance scale spreads the edge range
// symmetrically across it, with the isocontour at kSdfOnEdge (0.5 normalized).
constexpr float kSdfOversample = 2.0f;
constexpr int kSdfPadding = 16;
constexpr unsigned char kSdfOnEdge = 128;
constexpr float kSdfDistScale = 128.0f / static_cast<float>(kSdfPadding);

struct SdfGlyph {
    std::uint32_t codepoint = 0;
    int width = 0;
    int height = 0;
    int xoff = 0;
    int yoff = 0;
    float advance = 0;
    unsigned char* sdf = nullptr;
};

} // namespace

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

    stbtt_fontinfo info{};
    if (!stbtt_InitFont(&info, bytes.data(), 0)) return false;
    size_ = pixel_height;
    sdf_oversample_ = kSdfOversample;
    sdf_unit_ = kSdfDistScale / 255.0f;
    int raw_ascent = 0;
    stbtt_GetFontVMetrics(&info, &raw_ascent, nullptr, nullptr);
    ascent_ = static_cast<float>(raw_ascent) * stbtt_ScaleForPixelHeight(&info, pixel_height);

    const float sdf_scale = stbtt_ScaleForPixelHeight(&info, pixel_height * kSdfOversample);
    std::vector<SdfGlyph> glyphs;
    glyphs.reserve(192);
    for (const auto first : {32, 0x400}) {
        for (int i = 0; i < 96; ++i) {
            const int codepoint = first + i;
            int width = 0, height = 0, xoff = 0, yoff = 0;
            unsigned char* sdf = stbtt_GetCodepointSDF(&info, sdf_scale, codepoint, kSdfPadding, kSdfOnEdge,
                                                      kSdfDistScale, &width, &height, &xoff, &yoff);
            if (!sdf || width <= 0 || height <= 0) {
                if (sdf) stbtt_FreeSDF(sdf, nullptr);
                continue;
            }
            int advance = 0;
            stbtt_GetCodepointHMetrics(&info, codepoint, &advance, nullptr);
            SdfGlyph glyph;
            glyph.codepoint = static_cast<std::uint32_t>(codepoint);
            glyph.width = width;
            glyph.height = height;
            glyph.xoff = xoff;
            glyph.yoff = yoff;
            glyph.advance = static_cast<float>(advance) * sdf_scale / kSdfOversample;
            glyph.sdf = sdf;
            glyphs.push_back(glyph);
        }
    }
    if (glyphs.empty()) return false;

    stbrp_context pack{};
    std::vector<stbrp_node> nodes;
    std::vector<stbrp_rect> rects(glyphs.size() + 1);
    atlas_w_ = 0;
    for (const int side : {1024, 2048}) {
        nodes.assign(static_cast<std::size_t>(side), stbrp_node{});
        stbrp_init_target(&pack, side, side, nodes.data(), side);
        for (std::size_t i = 0; i < glyphs.size(); ++i) {
            rects[i].id = static_cast<int>(i);
            rects[i].w = glyphs[i].width;
            rects[i].h = glyphs[i].height;
        }
        rects[glyphs.size()].id = -1;
        rects[glyphs.size()].w = 2;
        rects[glyphs.size()].h = 2;
        if (stbrp_pack_rects(&pack, rects.data(), static_cast<int>(rects.size())) != 0) {
            atlas_w_ = atlas_h_ = side;
            break;
        }
    }
    if (atlas_w_ == 0) {
        for (auto& glyph : glyphs) stbtt_FreeSDF(glyph.sdf, nullptr);
        return false;
    }

    pixels_.assign(static_cast<std::size_t>(atlas_w_ * atlas_h_), 0);
    glyphs_.clear();
    for (const auto& rect : rects) {
        if (rect.id < 0) {
            for (int y = 0; y < 2; ++y)
                for (int x = 0; x < 2; ++x)
                    pixels_[static_cast<std::size_t>((rect.y + y) * atlas_w_ + rect.x + x)] = 255;
            white_u_ = (static_cast<float>(rect.x) + 0.5f) / static_cast<float>(atlas_w_);
            white_v_ = (static_cast<float>(rect.y) + 0.5f) / static_cast<float>(atlas_h_);
            continue;
        }
        const auto& glyph = glyphs[static_cast<std::size_t>(rect.id)];
        for (int y = 0; y < glyph.height; ++y)
            for (int x = 0; x < glyph.width; ++x)
                pixels_[static_cast<std::size_t>((rect.y + y) * atlas_w_ + rect.x + x)] =
                    glyph.sdf[static_cast<std::size_t>(y * glyph.width + x)];
        Glyph g;
        g.x0 = static_cast<float>(glyph.xoff) / kSdfOversample;
        g.y0 = static_cast<float>(glyph.yoff) / kSdfOversample;
        g.x1 = static_cast<float>(glyph.xoff + glyph.width) / kSdfOversample;
        g.y1 = static_cast<float>(glyph.yoff + glyph.height) / kSdfOversample;
        g.u0 = static_cast<float>(rect.x) / static_cast<float>(atlas_w_);
        g.v0 = static_cast<float>(rect.y) / static_cast<float>(atlas_h_);
        g.u1 = static_cast<float>(rect.x + rect.w) / static_cast<float>(atlas_w_);
        g.v1 = static_cast<float>(rect.y + rect.h) / static_cast<float>(atlas_h_);
        g.advance = glyph.advance;
        glyphs_[glyph.codepoint] = g;
    }
    for (auto& glyph : glyphs) stbtt_FreeSDF(glyph.sdf, nullptr);
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
