// =====================================================================
// Input.h - keyboard routing + text-field model (no OpenGL dependency, unit-tested)
//
// RULE: while a text field has focus EVERY printable character goes to that
// field. Global shortcuts are only offered the key when nothing is focused.
// There are deliberately no letter/digit shortcuts in the application at all;
// the router enforces the rule anyway. Typing "Raigad" yields exactly "Raigad".
// =====================================================================
#pragma once
#include <functional>
#include <string>

namespace ui {

class TextField {
public:
    explicit TextField(std::size_t maxLen = 200, bool numeric = false, bool password = false)
        : maxLen_(maxLen), numeric_(numeric), password_(password) {}
    const std::string& text() const { return text_; }
    void setText(const std::string& t);
    void setCursor(std::size_t position);
    void clear() { text_.clear(); cursor_ = 0; passwordVisible_ = false; }
    std::size_t cursor() const { return cursor_; }
    bool isPassword() const { return password_; }
    bool isPasswordVisible() const { return passwordVisible_; }
    void togglePasswordVisibility() { if (password_) passwordVisible_ = !passwordVisible_; }
    bool insertChar(char c);          // false if rejected (filter / length)
    bool insertText(const std::string& text);
    void backspace();
    void del();
    void moveLeft();
    void moveRight();
    void home() { cursor_ = 0; }
    void end() { cursor_ = text_.size(); }
    std::string display() const;      // masked when password
    bool changed = false;             // set on edit, cleared by the owner

private:
    std::string text_;
    std::size_t cursor_ = 0, maxLen_;
    bool numeric_, password_, passwordVisible_ = false;
};

enum class Key { NONE, BACKSPACE, DEL, LEFT, RIGHT, HOME, END, ENTER, TAB, ESCAPE, F5 };

class KeyRouter {
public:
    void focus(TextField* f) { focused_ = f; }
    void blur() { focused_ = nullptr; }
    TextField* focused() const { return focused_; }
    bool hasFocus() const { return focused_ != nullptr; }

    // global handler receives keys ONLY when no field is focused
    std::function<void(char)> onGlobalChar;
    std::function<void(Key)> onGlobalKey;
    std::function<void()> onSubmit;          // Enter inside a field
    std::function<void()> onTabNext;

    // returns true if the key was consumed by the focused field
    bool handleChar(char c);
    bool handlePaste(const std::string& text);
    bool handleKey(Key k);

private:
    TextField* focused_ = nullptr;
};

} // namespace ui
