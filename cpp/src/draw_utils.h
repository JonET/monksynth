#pragma once

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cgraphicspath.h"

#include <string>
#include <vector>

namespace MonkSynth {

// Fill a rounded rectangle. Shared by the overlay views.
inline void drawRoundRect(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &r,
                          VSTGUI::CCoord radius) {
    auto *path = ctx->createRoundRectGraphicsPath(r, radius);
    if (!path)
        return;
    ctx->drawGraphicsPath(path, VSTGUI::CDrawContext::kPathFilled);
    path->forget();
}

// Shorten |text| with a trailing ellipsis until it fits |maxWidth| in the
// context's current font. Cuts at UTF-8 character boundaries.
inline std::string ellipsize(VSTGUI::CDrawContext *ctx, std::string text,
                             VSTGUI::CCoord maxWidth) {
    static const char *kEllipsis = "\xE2\x80\xA6";
    if (ctx->getStringWidth(text.c_str()) <= maxWidth)
        return text;
    while (!text.empty()) {
        // Drop one UTF-8 character from the end.
        size_t i = text.size() - 1;
        while (i > 0 && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80)
            i--;
        text.erase(i);
        std::string candidate = text + kEllipsis;
        if (ctx->getStringWidth(candidate.c_str()) <= maxWidth)
            return candidate;
    }
    return kEllipsis;
}

// Word-wrap |text| to |maxWidth| in the context's current font, returning at
// most |maxLines| lines; the last one is ellipsized if text remains. Breaks
// at spaces, or between any two characters when a run has none (Japanese).
inline std::vector<std::string> wrapText(VSTGUI::CDrawContext *ctx, const std::string &text,
                                         VSTGUI::CCoord maxWidth, size_t maxLines) {
    std::vector<std::string> lines;
    size_t pos = 0;
    while (pos < text.size() && lines.size() < maxLines) {
        while (pos < text.size() && text[pos] == ' ')
            pos++;
        if (lines.size() + 1 == maxLines) {
            lines.push_back(ellipsize(ctx, text.substr(pos), maxWidth));
            break;
        }
        // Grow the line one UTF-8 character at a time, remembering the last
        // space so the break can fall between words.
        size_t end = pos, lastSpace = std::string::npos;
        while (end < text.size()) {
            size_t next = end + 1;
            while (next < text.size() && (static_cast<unsigned char>(text[next]) & 0xC0) == 0x80)
                next++;
            if (ctx->getStringWidth(text.substr(pos, next - pos).c_str()) > maxWidth)
                break;
            if (text[end] == ' ')
                lastSpace = end;
            end = next;
        }
        // Back up to the last space only when the break would split a Latin
        // word; CJK text can break between any two characters.
        auto isWordChar = [](char c) {
            auto u = static_cast<unsigned char>(c);
            return u < 0x80 && u != ' ';
        };
        if (end < text.size() && lastSpace != std::string::npos && lastSpace > pos &&
            isWordChar(text[end]) && isWordChar(text[end - 1]))
            end = lastSpace;
        if (end == pos) { // a single character wider than the line
            end = pos + 1;
            while (end < text.size() && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80)
                end++;
        }
        lines.push_back(text.substr(pos, end - pos));
        pos = end;
    }
    return lines;
}

} // namespace MonkSynth
