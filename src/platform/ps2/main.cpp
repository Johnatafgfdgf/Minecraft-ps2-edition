#include "mcps2/ps2_platform.hpp"
#include "mcps2/tick_clock.hpp"
#include <kernel.h>
#include <libpad.h>
#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    using namespace mcps2;
    if (!ps2::initialize()) { std::printf("Graphics initialization failed\n"); SleepThread(); return 1; }
    TickClock clock;
    uint64_t previous = ps2::now_us();
    const char* save_status = "SELECT: save diagnostics (optional path argument)";
    for (;;) {
        const uint64_t now = ps2::now_us();
        clock.advance(now - previous);
        previous = now;
        // Retain debt if a frame is slow. Future world work must not skip ticks.
        for (unsigned i = 0; i < 8 && clock.consume(); ++i) {}
        const auto input = ps2::poll_input();
        char counters[128];
        std::snprintf(counters, sizeof(counters), "Simulation: %llu ticks | pending: %llu us",
            static_cast<unsigned long long>(clock.tick()), static_cast<unsigned long long>(clock.pending_us()));
        if (input.pressed & PAD_SELECT) {
            if (argc > 1) save_status = ps2::write_file(argv[1], counters, std::strlen(counters)) ? "Diagnostics saved" : "Write failed";
            else save_status = "Supply a writable PS2 path as the first ELF argument";
        }
        ps2::begin_frame();
        ps2::text(28, 30, "Minecraft PS2 - Java 1.21.1 behavior port", 0x9cd67a);
        ps2::text(28, 66, "Platform bring-up / not a completed game");
        ps2::text(28, 106, counters);
        ps2::text(28, 144, input.connected ? "Controller connected" : "Waiting for controller", 0x9cd67a);
        ps2::text(28, 182, ps2::filesystem_ready() ? "PS2SDK filesystem initialized" : "Filesystem initialization failed");
        ps2::text(28, 224, save_status);
        ps2::end_frame();
    }
}
