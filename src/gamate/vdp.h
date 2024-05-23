#ifndef GAMATE_VDP_H
#define GAMATE_VDP_H
#include <cstdint>

#define GAMATE_SCREEN_WIDTH 160
#define GAMATE_SCREEN_HEIGHT 150

#define RGB565(r, g, b) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))
// this palette is taken from megaduck, from videos it looks similar
static const unsigned short palette_gamate[] = {
        RGB565(0x6B, 0xA6, 0x4A),
        RGB565(0x43, 0x7A, 0x63),
        RGB565(0x25, 0x59, 0x55),
        RGB565(0x12, 0x42, 0x4C),
};

void screen_update(uint16_t * screen);
uint8_t vdp_read();
void vdp_write(uint16_t address, uint8_t value);
#endif //GAMATE_VDP_H
