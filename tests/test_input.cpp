#include "frontend/opengl/UI/Input.h"
#include <cstdio>

using namespace ui;

static int checks = 0;
static int failures = 0;

#define CHECK(condition) \
    do { \
        ++checks; \
        if (!(condition)) { \
            ++failures; \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        } \
    } while (0)

int main() {
    TextField field(12);
    field.setText("ab");
    field.setCursor(1);
    CHECK(field.insertText("X\r\nY"));
    CHECK(field.text() == "aXYb");
    CHECK(field.cursor() == 3 && field.changed);

    TextField numeric(8, true);
    CHECK(numeric.insertText("12a3\n4"));
    CHECK(numeric.text() == "1234");

    TextField utf8(5);
    CHECK(utf8.insertText("abc\xc3\xa9x"));
    CHECK(utf8.text() == "abc\xc3\xa9");

    TextField noRoom(2);
    CHECK(noRoom.insertText("abcd"));
    CHECK(noRoom.text() == "ab");

    KeyRouter keys;
    CHECK(!keys.handlePaste("ignored"));
    keys.focus(&field);
    CHECK(keys.handlePaste("Z"));
    CHECK(field.text() == "aXYZb");
    keys.blur();
    CHECK(!keys.handlePaste("ignored"));

    std::printf("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
