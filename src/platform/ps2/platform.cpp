#include "mcps2/ps2_platform.hpp"
#include <gsKit.h>
#include <dmaKit.h>
#include <gsFontM.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <smod.h>
#include <unistd.h>
#include <libpad.h>
#include <timer.h>
#include <fcntl.h>
#include <cstdio>

namespace mcps2::ps2 {
namespace {
GSGLOBAL* graphics = nullptr;
GSFONTM* font = nullptr;
char pad_buffer[256] __attribute__((aligned(64)));
bool pad_open = false, analog_requested = false, fs_ready = false;
uint16_t previous = 0;
int ensure_module(const char* name, const char* path) {
    smod_mod_info_t resident{};
    const int id = smod_get_mod_by_name(name, &resident);
    return id > 0 ? id : SifLoadModule(path, 0, nullptr);
}
}

bool initialize() {
    SifInitRpc(0);
    // Preserve the loader's IOP filesystem modules; do not reset its host/mass device.
    // PS2SDK newlib initializes its device bridge at startup. Use its POSIX API.
    const int probe = open("rom0:ROMVER", O_RDONLY);
    fs_ready = probe >= 0;
    if (probe >= 0) close(probe);
    const int sio = ensure_module("sio2man", "rom0:XSIO2MAN");
    const int pad = ensure_module("padman", "rom0:XPADMAN");
    if (sio >= 0 && pad >= 0) {
        if (padInit(0) >= 0) pad_open = padPortOpen(0, 0, pad_buffer) != 0;
    } else {
        std::printf("Controller modules: XSIO2MAN=%d XPADMAN=%d\n", sio, pad);
    }
    graphics = gsKit_init_global();
    if (!graphics) return false;
    graphics->Width = 640;
    graphics->Height = graphics->Mode == GS_MODE_PAL ? 512 : 448;
    graphics->PSM = GS_PSM_CT16;
    graphics->ZBuffering = GS_SETTING_OFF;
    graphics->DoubleBuffering = GS_SETTING_ON;
    graphics->PrimAlphaEnable = GS_SETTING_ON;
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsKit_init_screen(graphics);
    gsKit_mode_switch(graphics, GS_ONESHOT);
    // Font is obtained at runtime from the console BIOS, not from Minecraft.
    font = gsKit_init_fontm();
    if (font && gsKit_fontm_upload(graphics, font) < 0) font = nullptr;
    return true;
}

Input poll_input() {
    Input input;
    if (!pad_open) return input;
    const int state = padGetState(0, 0);
    if (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1) {
        previous = 0;
        analog_requested = false;
        return input;
    }
    if (!analog_requested) {
        const int modes = padInfoMode(0, 0, PAD_MODETABLE, -1);
        for (int i = 0; i < modes; ++i) {
            if (padInfoMode(0, 0, PAD_MODETABLE, i) == PAD_TYPE_DUALSHOCK) {
                padSetMainMode(0, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);
                break;
            }
        }
        analog_requested = true;
        return input;
    }
    padButtonStatus status{};
    if (!padRead(0, 0, &status)) return input;
    input.connected = true;
    input.held = static_cast<uint16_t>(status.btns ^ 0xffffu);
    input.pressed = input.held & static_cast<uint16_t>(~previous);
    previous = input.held;
    input.left_x = status.ljoy_h; input.left_y = status.ljoy_v;
    input.right_x = status.rjoy_h; input.right_y = status.rjoy_v;
    return input;
}

uint64_t now_us() {
    u32 seconds = 0, micros = 0;
    TimerBusClock2USec(GetTimerSystemTime(), &seconds, &micros);
    return uint64_t(seconds) * 1000000u + micros;
}
void begin_frame() { gsKit_clear(graphics, GS_SETREG_RGBAQ(18, 23, 32, 0x80, 0)); }
void text(float x, float y, const char* message, uint32_t rgb) {
    if (font) gsKit_fontm_print_scaled(graphics, font, x, y, 1, 0.65f,
        GS_SETREG_RGBAQ((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, 0x80, 0), message);
}
void end_frame() { gsKit_queue_exec(graphics); gsKit_sync_flip(graphics); }
bool filesystem_ready() { return fs_ready; }
bool write_file(const char* path, const void* data, size_t bytes) {
    if (!fs_ready || !path || bytes > 0x7fffffffu) return false;
    const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return false;
    const auto* cursor = static_cast<const unsigned char*>(data);
    size_t done = 0;
    while (done < bytes) {
        const int wrote = write(fd, cursor + done, bytes - done);
        if (wrote <= 0) { close(fd); return false; }
        done += size_t(wrote);
    }
    return close(fd) >= 0;
}
}
