/* See LICENSE file for license details */

/* Standard library includes */
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#include <windows.h>

#include "MiniFB.h"

#include "bios.h"

static uint8_t ROM[512 << 10] = { 0 };

static uint8_t RAM[10239] = { 0 };

static uint16_t SCREEN[32][48] = { 0 };

static uint8_t *key_status = (uint8_t *) mfb_keystatus();

static inline void readfile(const char *pathname, uint8_t *dst) {
    size_t rom_size;

    FILE *file = fopen(pathname, "rb");
    fseek(file, 0, SEEK_END);
    rom_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    fread(dst, sizeof(uint8_t), rom_size, file);
    fclose(file);
}

#define AUDIO_FREQ 44100
#define AUDIO_BUFFER_LENGTH ((AUDIO_FREQ /60 +1) * 2)

int16_t audiobuffer[AUDIO_BUFFER_LENGTH] = { 0 };

DWORD WINAPI SoundThread(LPVOID lpParam) {
    WAVEHDR waveHeaders[4];

    WAVEFORMATEX format = { 0 };
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = AUDIO_FREQ;
    format.wBitsPerSample = 16;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    HANDLE waveEvent = CreateEvent(NULL, 1, 0, NULL);

    HWAVEOUT hWaveOut;
    waveOutOpen(&hWaveOut, WAVE_MAPPER, &format, (DWORD_PTR) waveEvent, 0, CALLBACK_EVENT);

    for (size_t i = 0; i < 4; i++) {
        int16_t audio_buffers[4][AUDIO_BUFFER_LENGTH * 2];
        waveHeaders[i] = {
                .lpData = (char *) audio_buffers[i],
                .dwBufferLength = AUDIO_BUFFER_LENGTH * 2,
        };
        waveOutPrepareHeader(hWaveOut, &waveHeaders[i], sizeof(WAVEHDR));
        waveHeaders[i].dwFlags |= WHDR_DONE;
    }
    WAVEHDR *currentHeader = waveHeaders;


    while (true) {
        if (WaitForSingleObject(waveEvent, INFINITE)) {
            fprintf(stderr, "Failed to wait for event.\n");
            return 1;
        }

        if (!ResetEvent(waveEvent)) {
            fprintf(stderr, "Failed to reset event.\n");
            return 1;
        }

// Wait until audio finishes playing
        while (currentHeader->dwFlags & WHDR_DONE) {
//            PSG_calc_stereo(&psg, audiobuffer, AUDIO_BUFFER_LENGTH);
            memcpy(currentHeader->lpData, audiobuffer, AUDIO_BUFFER_LENGTH * 2);
            waveOutWrite(hWaveOut, currentHeader, sizeof(WAVEHDR));

            currentHeader++;
            if (currentHeader == waveHeaders + 4) { currentHeader = waveHeaders; }
        }
    }
    return 0;
}


static inline uint8_t read_register(uint8_t address) {
    //printf("REGISTER READ %02x : %02x\r\n", address, RAM[address]);

    switch (address) {
        // IO
        case 0x00:
            return key_status[0x0d] ? 0b11111101 : 0xff;
        case 0x01: {
            uint8_t buttons = 0xff;
            if (key_status[0x26]) buttons ^= 0b10000;
            if (key_status[0x28]) buttons ^= 0b100000;
            if (key_status[0x25]) buttons ^= 0b1000000;
            if (key_status[0x27]) buttons ^= 0b10000000;

            if (key_status['Z']) buttons ^= 0b100;
            if (key_status['X']) buttons ^= 0b1000;

            if (key_status[0x20]) buttons ^= 0b10;

            return buttons;
        }
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
            break;

            // AUDIO - square waves
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
            break;

            // AUDIO - pwm
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
            /// 0x18..0x1f    (ff)  Never used. Reads as 0ffh regardless of value written.
            // TIMER
        case 0x20:
        case 0x21:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
            break;

            // DMA
        case 0x28:
        case 0x29:
        case 0x2a:
        case 0x2b:
        case 0x2c:
        case 0x2d:
        case 0x2e:
        case 0x2f:
        case 0x30:
        case 0x31:
            break;
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
            //printf("Bank select %x\r\n", address);
            break;

        case 0x36:
        case 0x37:
        case 0x38:
        case 0x39:
        case 0x3A:
        case 0x3B:
            break;

            // Interrupts
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
            break;

            // LCD
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x46:
        case 0x48:
        case 0x49:
        case 0x4a:
        case 0x4b:
            break;
        case 0x4c:
            //printf("protection?????????? \r\n");
            return 6;
            break;
        case 0x4d:
        case 0x4e:
            break;

            // OTHER
        case 0x4f:
        case 0x50:
        case 0x51:
        case 0x52:
        case 0x53:

        case 0x54:
            // ..
        case 0x5F:
            break;
            break;
        default:
            break;
    }

    return RAM[address];
}


uint8_t interrupts_enabled = false;

static inline void write_register(uint8_t address, uint8_t value) {
    //printf("REGISTER WRITE %02x : %02x 0x%04x \r\n", address, value, cpu.PC.W);
    switch (address) {
        // TIMER
/* 0020: (01)  Timer 0.  Runs off the 32.768KHz oscillator.  This sets the divisor
            for the one shot mode below. This is only written to once.

            0 - mode
            1 - mode
            2 - mode
            3 - mode
            4 - mode
            5 - always reads as 1
            6 - always reads as 1
            7 - always reads as 1

            mode:

            00 - disabled
            01 - 2Hz
            02-03 - 8Hz
            04-07 - 64Hz
            08-0F - 256Hz
            10-1F - 2KHz
 */
        case 0x20:
            printf("0x20 Timer 0 divider set to %x\n", value & 0b11111);
            break;
/* 0021: (??)  One shot interrupt timer.  If bit 7 is written set, then the one shot
            interrupt is started.  The interrupt will occur when timer 0
            overflows.  The GK will write to this register with 80h to keep the
            interrupts going in the interrupt handler.
*/
        case 0x21:
            printf("0x21 One shot interrupt timer: %s PC:0x%04x\n", value & 0b10000000 ? "started" : "stopped", 9);
            break;
/* 0023: (C0)  Timer 1 enable?  Writing here enables timer 1 if bit 6 is set and
            bit 5 is clear. i.e. C0-DF will make the timer run.  anything else
            seems to stop the timer.  Probably selects a timer mode of some kind
            like capture/compare and PWM which is not used on GK.  C0h is
            written here once and never touched again.
*/
        case 0x23:
            printf("0x23 Timer 1 %s\n", (value >= 0xC0 && value <= 0xDF) ? "enabled" : "disabled");
            break;
/* 0024: (F4)  TCON reg for timer 1. The time base is the system clock (6MHz).
            It is either divided down by the prescaler at 0025, or it is just
            divided by 256.  GK writes 34h here once.

            0 - Step size
            1 - Step size
            2 - Step size
            3 - ? - toggling this doesn't seem to affect anything
            4 - 1 = use prescaler at 0025, 0 = divide by 256
            5 - 1 = enable timer, 0 = disable timer
            6 - ? - reads as 1
            7 - ? - reads as 1

            step size:

            0 - /65536
            1 - /32768
            2 - /8192
            3 - /2048
            4 - /256
            5 - /32
            6 - /8
            7 - /2
 */
        case 0x24:
            /* default value is 0x34:
             * stepsize /256
             * use prescaler at 0025
             * timer enabled
             */
/*0025: (1F)  Timer 1 prescaler.   Divides the CPU clock by this value.  It is a
            "count up" style prescaler.  FFh = divide by 1, 00h = divide by 256.
            Reading returns the instantaneous timer value.   GK writes 49h
            here once which corresponds to a divisor of 183 decimal.

            A note about timer 1:

            The interrupt will occur when the timer chain overflows.  This
            means on the GK, the 6MHz clock is first divided by the prescaler,
            then runs through the stepsize dividers.  Once the stepsize divider
            overflows, the interrupt is generated.  i.e., on the GK they load
            49h into the prescaler and enable it, then select /256 mode.
            This produces around a 128Hz interrupt rate.  The actual rate is:
            6MHz / (256-73) / 256 = 128.07377Hz.
            */
        case 0x25:
        case 0x26:
        case 0x27:
            printf("0x%x Timer %x\n", address, value);
            break;

            // Banks switching
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
//            printf("0x%02x BANKING 0x%02x\n", address, value);
            break;
            // Interrupts
/* 003C: (4A)  Interrupt pending register.  Writing 00 here will reset the
            interrupts represented by register 003E.
*/
        case 0x3C:
            if (value == 0) interrupts_enabled = false;
            printf("0x3C Pending Interrupt %d PC:0x%04x\n", value, 0);
            break;
/* 003E: (34)  Interrupt enable register.  This enables many of the interrupt
            sources.  Reading seems to return interrupt sources that are
            pending.

            0 - ? not sure, have not gotten it to generate an interrupt
            1 - square 2 / PWM timer  int B
            2 - timer controlled by reg 0023/0024/0025  int A
            3 - Timer controlled by reg 0026  int 9
            4 - interrupt on IO change at port 0000 int 8
            5 - timer 0 interrupt / one shot int 7
            6 - Vblank. Each time the screen redraws, this interrupt fires int A
            7 - ? not sure, have not gotten it to generate an interrupt
 */
        case 0x3E:
            printf("0x3E Enabled Interrupts 0x%02x\n", value);
            if ((value >> 1) & 1) printf("1 - square 2 / PWM timer  int B\n");
            if ((value >> 2) & 1) printf("2 - timer controlled by reg 0023/0024/0025  int A\n");
            if ((value >> 3) & 1) printf("3 - Timer controlled by reg 0026  int 9\n");
            if ((value >> 4) & 1) printf("4 - interrupt on IO change at port 0000 int 8\n");
            if ((value >> 5) & 1) printf("5 - timer 0 interrupt / one shot int 7\n");
            if ((value >> 6) & 1) printf("6 - Vblank. Each time the screen redraws, this interrupt fires int A\n");
            interrupts_enabled = value;
            break;
            // LCD
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x46:
        case 0x48:
        case 0x49:
        case 0x4a:
        case 0x4b:
        case 0x4c:
        case 0x4d:
        case 0x4e:
            printf("0x%02x LCD 0x%02x\n", address, value);
            break;
        default:
            break;
    }

    RAM[address] = value;
}

extern "C" uint8_t read6502(uint16_t address) {
    //printf("rd %04x\n", address);
    if (address <= 0x007F) {
        return read_register(address);
    }

    if (address <= 0x27FF) {
        return RAM[address];
    }

    if (address >= 0x4000 && address <= 0x7FFF) {
        if (address >= 0x7FE2) {
            //printf("Interrupt 0x%04x\r\n", address);
        }

        int chip = RAM[0x33] & 1;
        int bank = RAM[0x32] ^ 1;
        int32_t offset = (address - 0x4000) + bank * 0x4000;
        //printf("bank0 a %04x rom %d  bank %d PC %04x\n", address, chip, RAM[0x32] ^ 1, 1);
        return chip ? ROM[offset] : BIOS[offset];
    }

    if (address >= 0x8000) {
        int32_t offset = (address - 0x8000) + ((RAM[0x34] & 0x7f) * 0x8000);
        return (RAM[0x34] & 0x80) ? ROM[offset] : BIOS[offset];
    }

    // Open bus
    return 0x00;

/*
    // INPUT
    if (address == 0x4400) {
        uint8_t buttons = 0xff;

        if (key_status[0x26]) buttons ^= 0b1;
        if (key_status[0x28]) buttons ^= 0b10;
        if (key_status[0x25]) buttons ^= 0b100;
        if (key_status[0x27]) buttons ^= 0b1000;
        if (key_status['Z']) buttons ^= 0b10000;
        if (key_status['X']) buttons ^= 0b100000;
        if (key_status[0x0d]) buttons ^= 0b1000000;
        if (key_status[0x20]) buttons ^= 0b10000000;

        return buttons;
    }
*/
}

extern "C" void write6502(uint16_t address, uint8_t value) {
    if (address <= 0x007F) {
        write_register(address, value);
        return;
    }
    if (address >= 0x0200 && address <= 0x03FF && value > 0) {
        //printf("LCD 0x%04x 0x%02x \n", address, value);
    }

    if (address <= 0x27FF) {
        RAM[address] = value;
        return;
    }

}
extern "C" void reset6502();
extern "C" void exec6502(uint32_t tickcount);
extern "C" void irq6502(uint16_t vector);
#define RGB565(r, g, b) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))
static const uint16_t gameking_palette[] = {
        RGB565(0x8C, 0xCE, 0x94),
        RGB565(0x6B, 0x9C, 0x63),
        RGB565(0x50, 0x65, 0x41),
        RGB565(0x18, 0x42, 0x21),
};

// 7FEE/7FEF : int 7   timer 0 interrupt (enabled via 003E.5)
#define TIMER0_INTERRUPT 0x7FEE
// 7FF0/7FF1 : int 8   IO change interrupt (enabled via 003E.4)
#define IO_INTERRUPT 0x7FF0
// 7FF2/7FF3 : int 9   timer 2 interrupt (enable via 003E.3)
#define TIMER2_INTERRUPT 0x7FF2
// 7FF4/7FF5 : int A   timer 1 / VBL interrupt (enable via 003E.2 / 003E.6)
#define TIMER1_INTERRUPT 0x7FF4
// 7FF6/7FF7 : int B   square 2 / PWM timer (enable via 003E.1)
#define SQUARE2_TIMER_INTERRUPT 0x7FF6

int main(int argc, char **argv) {
    int scale = 20;

    if (!argv[1]) {
        printf("Usage: gamate.exe <rom.bin> [scale_factor]\n");
        exit(-1);
    }

    if (argv[2]) {
        scale = atoi(argv[2]);
    }

    if (!mfb_open("Gameking", 48, 32, scale))
        return 0;

//    CreateThread(NULL, 0, SoundThread, NULL, 0, NULL);

//    readfile(argv[1], ROM);

    memset(RAM, 0x00, sizeof(RAM));
    RAM[0x32] = RAM[0x33] = 0;


    reset6502();

    for (;;) {
        irq6502(TIMER0_INTERRUPT);
        exec6502(65535);
        irq6502(TIMER1_INTERRUPT); // VBLANK
//        exec6502(32768);


//        irq6502(TIMER0_INTERRUPT);
//        irq6502(SQUARE2_TIMER_INTERRUPT);

        uint16_t offset =  *((uint16_t * )&RAM[0x40]);
        for (int y = 31, i = 0; i < 32; i++, y--) {
            for (int x = 0, j = offset; j < offset+48 / 4; x += 4, j++) {
                uint8_t data = RAM[j + i * 12];
                SCREEN[y][x + 3] = gameking_palette[data & 3];
                SCREEN[y][x + 2] = gameking_palette[(data >> 2) & 3];
                SCREEN[y][x + 1] = gameking_palette[(data >> 4) & 3];
                SCREEN[y][x + 0] = gameking_palette[(data >> 6) & 3];
            }
        }


        if (mfb_update(SCREEN, 60) == -1)
            exit(1);
    }
}
