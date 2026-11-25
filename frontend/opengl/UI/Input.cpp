#include "Input.h"
#include <algorithm>

namespace ui {

namespace {
bool isContinuation(unsigned char c) { return (c & 0xC0) == 0x80; }

std::size_t utf8CharLength(const std::string& text, std::size_t offset) {
    unsigned char lead = static_cast<unsigned char>(text[offset]);
    std::size_t length = 1;
    if ((lead & 0xE0) == 0xC0) length = 2;
    else if ((lead & 0xF0) == 0xE0) length = 3;
    else if ((lead & 0xF8) == 0xF0) length = 4;
    if (offset + length > text.size()) return 1;
    for (std::size_t i = 1; i < length; ++i)
        if (!isContinuation(static_cast<unsigned char>(text[offset + i]))) return 1;
    return length;
}
}

void TextField::setText(const std::string& t) {
    text_ = t.substr(0, maxLen_);
    cursor_ = text_.size();
}

void TextField::setCursor(std::size_t position) {
    cursor_ = std::min(position, text_.size());
    while (cursor_ > 0 && cursor_ < text_.size() && isContinuation(static_cast<unsigned char>(text_[cursor_]))) --cursor_;
}

void TextField::moveLeft() {
    if (cursor_ == 0) return;
    --cursor_;
    while (cursor_ > 0 && isContinuation(static_cast<unsigned char>(text_[cursor_]))) --cursor_;
}

void TextField::moveRight() {
    if (cursor_ >= text_.size()) return;
    ++cursor_;
    while (cursor_ < text_.size() && isContinuation(static_cast<unsigned char>(text_[cursor_]))) ++cursor_;
}

bool TextField::insertChar(char c) {
    unsigned char u = static_cast<unsigned char>(c);
    if (u < 32 || u == 127) return false;
    if (numeric_ && (u < '0' || u > '9')) return false;
    if (text_.size() >= maxLen_) return false;
    text_.insert(text_.begin() + static_cast<long>(cursor_), c);
    ++cursor_;
    changed = true;
    return true;
}

bool TextField::insertText(const std::string& text) {
    bool inserted = false;
    for (std::size_t i = 0; i < text.size();) {
        std::size_t length = utf8CharLength(text, i);
        unsigned char first = static_cast<unsigned char>(text[i]);
        if (first < 32 || first == 127 || (numeric_ && (length != 1 || first < '0' || first > '9'))) {
            i += length;
            continue;
        }
        if (text_.size() + length > maxLen_) break;
        text_.insert(cursor_, text, i, length);
        cursor_ += length;
        i += length;
        inserted = true;
    }
    if (inserted) changed = true;
    return inserted;
}

void TextField::backspace() {
    if (cursor_ == 0) return;
    std::size_t start = cursor_ - 1;
    while (start > 0 && isContinuation(static_cast<unsigned char>(text_[start]))) --start;
    text_.erase(start, cursor_ - start);
    cursor_ = start;
    changed = true;
}

void TextField::del() {
    if (cursor_ >= text_.size()) return;
    std::size_t end = cursor_ + 1;
    while (end < text_.size() && isContinuation(static_cast<unsigned char>(text_[end]))) ++end;
    text_.erase(cursor_, end - cursor_);
    changed = true;
}

std::string TextField::display() const {
    return password_ && !passwordVisible_ ? std::string(text_.size(), '*') : text_;
}

bool KeyRouter::handleChar(char c) {
    if (focused_) {
        // control characters that GLUT delivers through the char callback
        if (c == 8) { focused_->backspace(); return true; }
        if (c == 127) { focused_->del(); return true; }
        if (c == 13) { if (onSubmit) onSubmit(); return true; }
        if (c == 9) { if (onTabNext) onTabNext(); return true; }
        if (c == 27) { blur(); return true; }
        focused_->insertChar(c);      // printable: ALWAYS text, never a shortcut
        return true;
    }
    if (onGlobalChar) onGlobalChar(c);
    return false;
}

bool KeyRouter::handlePaste(const std::string& text) {
    if (!focused_) return false;
    focused_->insertText(text);
    return true;
}

bool KeyRouter::handleKey(Key k) {
    if (focused_) {
        switch (k) {
            case Key::LEFT: focused_->moveLeft(); return true;
            case Key::RIGHT: focused_->moveRight(); return true;
            case Key::HOME: focused_->home(); return true;
            case Key::END: focused_->end(); return true;
            case Key::DEL: focused_->del(); return true;
            case Key::BACKSPACE: focused_->backspace(); return true;
            default: break;
        }
    }
    if (onGlobalKey) onGlobalKey(k);
    return false;
}

} // namespace ui
