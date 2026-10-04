#pragma once
#include <cstddef>
#include <cstdint>

namespace mcps2::ps2 {
struct Input {
    uint16_t held = 0, pressed = 0;
    uint8_t left_x = 128, left_y = 128, right_x = 128, right_y = 128;
    bool connected = false;
};
bool initialize();
Input poll_input();
uint64_t now_us();
void begin_frame();
void text(float x, float y, const char* message, uint32_t rgb = 0xffffff);
void end_frame();
bool write_file(const char* path, const void* data, size_t bytes);
bool filesystem_ready();
}
