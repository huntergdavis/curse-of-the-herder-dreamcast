/* Curse of the Herder, Dreamcast port: boot proof.
 * Direct framebuffer writes (no PVR), the most robust path under a high-level
 * BIOS: pasture-green screen, a fence-brown pen, a blinking wool-white sheep.
 * Press Start to exit.
 */
#include <kos.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

static uint16 rgb565(int r, int g, int b) {
    return (uint16)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void fill_rect(int x, int y, int w, int h, uint16 c) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            vram_s[j * 640 + i] = c;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    vid_set_mode(DM_640x480, PM_RGB565);
    const uint16 grass = rgb565(0x94, 0xac, 0x4c);
    const uint16 fence = rgb565(0x6b, 0x4a, 0x2b);
    const uint16 wool  = rgb565(0xf6, 0xf2, 0xe6);
    const uint16 ink   = rgb565(0x2b, 0x26, 0x20);
    int frame = 0;
    for (;;) {
        maple_device_t *cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        if (cont) {
            cont_state_t *st = (cont_state_t *)maple_dev_status(cont);
            if (st && (st->buttons & CONT_START)) break;
        }
        fill_rect(0, 0, 640, 480, grass);
        fill_rect(220, 140, 200, 8, fence);
        fill_rect(220, 332, 200, 8, fence);
        fill_rect(220, 140, 8, 200, fence);
        fill_rect(412, 140, 8, 200, fence);
        if ((frame / 30) % 2 == 0) {
            fill_rect(300, 220, 40, 30, wool);
            fill_rect(336, 226, 8, 12, ink);
        }
        vid_waitvbl();
        frame++;
    }
    return 0;
}
