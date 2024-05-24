#pragma GCC optimize("Ofast")
// license:BSD-3-Clause
// copyright-holders:David Haywood, Peter Wilhelmsen, Kevtris

/*
    Notes:

    Some games are glitchy, most of these glitches are verified to happen on hardware
    for example

    Badly flipped sprites in Tornado and Insect War
    Heavy flickering sprites in many games

    Most of these issues are difficult to notice on real hardware due to the poor
    quality display.

    Thanks to Kevtris for the documentation on which this implementation is based
    (some comments taken directly from this)
    http://blog.kevtris.org/blogfiles/Gamate%20Inside.txt

    ToDo:

    Emulate vram pull / LCD refresh timings more accurately.
    Interrupt should maybe be in here, not in drivers/gamate.cpp?
    Verify both Window modes act the same as hardware.
*/
#include <cstdint>
#include <cstring>
#include "vdp.h"

int m_vramaddress;
int m_bitplaneselect;
int m_scrollx;
int m_scrolly;
int m_window;
int m_swapplanes;
int m_incrementdir;
int m_displayblank;

alignas(4) uint8_t VRAM[16384];

static inline void increment_vram_address() {
    if (m_incrementdir)
        m_vramaddress += 0x20;
    else
        m_vramaddress++;

    m_vramaddress &= 0x1fff;
}


static inline void lcdcon_w(uint8_t data) {
    /*
    NXWS ???E
    E: When set, stops the LCD controller from refreshing the LCD.  This can
       damage the LCD material because the invert signal is no longer toggling,
       and the pixel/frame/row clocks/pulses are not being output.
    S: Swap plane bits.  When set, flip bit planes 0 and 1.
    W: D0-DF is mapped in at rows 00-0Fh at the top of the screen, with no
       X scroll for those rows. (see window bit info below)
    X: When clear the video address increments by 1. When set, it increments
       by 32.
    N: When set, clears the LCD by blanking the data.  The LCD refresh still occurs.
    */
    m_displayblank = (data & 0x80);
    m_incrementdir = (data & 0x40);
    m_window = (data & 0x20);
    m_swapplanes = (data & 0x10);
    // setting data & 0x01 is bad
}

static inline void xscroll_w(uint8_t data) {
    /*
    XXXX XXXX
    X: 8 bit Xscroll value
    */
    m_scrollx = data;
}

static inline void yscroll_w(uint8_t data) {
    /*
    YYYY YYYY
    Y: 8 bit Yscroll value
    */
    m_scrolly = data;
}

static inline void xpos_w(uint8_t data) {
    /*
    BxxX XXXX
    B: Bitplane. 0 = lower (bitplane 0), 1 = upper (bitplane 1)
    X: 5 lower bits of the 13 bit VRAM address.
    */
    m_bitplaneselect = (data & 0x80) >> 7;
    m_vramaddress = (m_vramaddress & 0x3fe0) | (data & 0x1f);
}

static inline void ypos_w(uint8_t data) {
    /*
    YYYY YYYY
    Y: 8 upper bits of 13 bit VRAM address.
    */
    m_vramaddress = (m_vramaddress & 0x001f) | (data << 5);;
}

uint8_t vdp_read() {
    uint16_t address = m_vramaddress << 1;

    if (m_bitplaneselect)
        address += 1;

    uint8_t ret = VRAM[address];

    increment_vram_address();

    return ret;
}

static inline void vram_w(uint8_t data) {
    uint16_t address = m_vramaddress << 1;

    if (m_bitplaneselect)
        address += 1;

    VRAM[address] = data;

    increment_vram_address();
}

void vdp_write(uint16_t address, uint8_t value) {
    switch (address & 7) {
        case 1:
            return lcdcon_w(value);
        case 2:
            return xscroll_w(value);
        case 3:
            return yscroll_w(value);
        case 4:
            return xpos_w(value);
        case 5:
            return ypos_w(value);
        case 7:
            return vram_w(value);
    }
}

static inline void get_real_x_and_y(int &ret_x, int &ret_y, int scanline) {
    /* the Gamate video has 2 'Window' modes,
       Mode 1 is enabled with an actual register
       Mode 2 is enabled automatically based on the yscroll value

       both modes seem designed to allow for a non-scrolling status bar at
       the top of the display.
    */

    if (m_scrolly < 0xc8) {
        ret_y = scanline + m_scrolly;

        if (ret_y >= 0xc8)
            ret_y -= 0xc8;

        ret_x = m_scrollx;

        if (m_window) /* Mode 1 Window */
        {
            if (scanline < 0x10) {
                ret_x = 0;
                ret_y = 0xd0 + scanline;
            }
        }
    } else /* Mode 2, do any games use this ? does above Window logic override this if enabled? */
    {
        ret_x = m_scrollx;

        /*
            Using Yscroll values of C8-CF, D8-DF, E8-EF, and F8-FF will result in the same
            effect as if a Yscroll value of 00h were used.
        */
        if (m_scrolly & 0x08) // values of C8-CF, D8-DF, E8-EF, and F8-FF
        {
            ret_y = 0x00;
            ret_x = m_scrollx;
        } else {
            /*
                Values D0-D7, E0-E7, and F0-F7 all produce a bit more useful effect.  The upper
                1-8 scanlines will be pulled from rows F8-FFh in VRAM (i.e. 1F00h = row F8h).

                If F0 is selected, then the upper 8 rows will be the last 8 rows in VRAM-
                1F00-1FFFh area.  If F1 is selected, the upper 8 rows will be the last 7 rows
                in VRAM and so on.  This special window area DOES NOT SCROLL with X making it
                useful for status bars.  I don't think any games actually used it, though.
            */
            int fixedscanlines = m_scrolly & 0x7;

            if (scanline <= fixedscanlines) {
                ret_x = 0;
                ret_y = 0xf8 + scanline + (7 - fixedscanlines);
            } else {
                // no yscroll in this mode?
                ret_x = m_scrollx;
                ret_y = scanline;// +m_scrolly;

                //if (ret_y >= 0xc8)
                //  ret_y -= 0xc8;
            }

        }
    }
}

static inline int get_pixel_from_vram(int x, int y) {
    x &= 0xff;
    y &= 0xff;

    int x_byte = x >> 3;
    x &= 0x7; // x pixel;

    int address = ((y * 0x20) + x_byte) << 1;

    int plane0 = (VRAM[address] >> (7 - x)) & 0x1;
    int plane1 = (VRAM[address + 1] >> (7 - x)) & 0x1;

    if (!m_swapplanes)
        return plane0 | (plane1 << 1);
    else
        return plane1 | (plane0 << 1); // does any game use this?
}


uint8_t ghosting_buffer[160 * 160] = { 0 };
void screen_update(uint16_t *screen) {
    int real_x, real_y;

    if (m_displayblank) {
        memset(screen, 0x00, (GAMATE_SCREEN_WIDTH*GAMATE_SCREEN_HEIGHT) * sizeof(uint16_t));
        return;
    }

    for (int scanline = 0; scanline < GAMATE_SCREEN_HEIGHT; scanline++) {
        get_real_x_and_y(real_x, real_y, scanline);

        for (int x = 0; x < GAMATE_SCREEN_WIDTH; x++) {
            int color = get_pixel_from_vram(x + real_x, real_y) << 4;
            if (0) {
                int prev_color = ghosting_buffer[scanline * GAMATE_SCREEN_WIDTH + x] - 2;

                if (color < prev_color) color = prev_color;
                ghosting_buffer[scanline * GAMATE_SCREEN_WIDTH + x] = color;
            }

            screen[scanline * GAMATE_SCREEN_WIDTH + x] = palette_gamate[ color >> 4 ];
        }
    }
}
