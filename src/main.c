/* Curse of the Herder, Dreamcast port: boot proof.
 * Clears the screen to pasture green, draws a pen-shaped rectangle and a
 * blinking sheep-white square, prints to the serial console, and exits on Start.
 */
#include <kos.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

static void fill_rect(uint16 *vram, int x, int y, int w, int h, uint16 colour) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            vram[j * 640 + i] = colour;
}

static uint16 rgb565(int r, int g, int b) {
    return (uint16)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    vid_set_mode(DM_640x480, PM_RGB565);
    printf("Curse of the Herder (Dreamcast): boot proof. Press Start to exit.\n");
    const uint16 grass = rgb565(0x94, 0xac, 0x4c);
    const uint16 fence = rgb565(0x6b, 0x4a, 0x2b);
    const uint16 wool  = rgb565(0xf6, 0xf2, 0xe6);
    int frame = 0;
    for (;;) {
        maple_device_t *cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        if (cont) {
            cont_state_t *st = (cont_state_t *)maple_dev_status(cont);
            if (st && (st->buttons & CONT_START)) break;
        }
        fill_rect(vram_s, 0, 0, 640, 480, grass);
        /* The pen: a hollow rectangle of fence colour. */
        fill_rect(vram_s, 220, 140, 200, 8, fence);
        fill_rect(vram_s, 220, 332, 200, 8, fence);
        fill_rect(vram_s, 220, 140, 8, 200, fence);
        fill_rect(vram_s, 412, 140, 8, 200, fence);
        /* One sheep, blinking, because it is the sim's job to move it and the sim is not here yet. */
        if ((frame / 30) % 2 == 0) fill_rect(vram_s, 300, 220, 40, 30, wool);
        vid_waitvbl();
        frame++;
    }
    printf("Goodbye from the pasture.\n");
    return 0;
}
