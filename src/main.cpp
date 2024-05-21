/* See LICENSE file for license details */

/* Standard library includes */
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#include <windows.h>

#include "MiniFB.h"

extern "C" {
#include "m6502/m6502.h"
}
#include "emu2149/emu2149.h"
#include "gamate/vdp.h"
#include "gamate/bios.h"

static PSG psg;
static m6502 cpu;

static uint8_t ROM[512 << 10];

static uint8_t RAM[10239] = { 0xFF };

static uint16_t SCREEN[150][160];

static uint8_t *key_status = (uint8_t *) mfb_keystatus();

static int bank0_offset = 0;
static int bank1_offset = 0x4000;
static uint8_t protection = 0;


static inline void readfile(const char *pathname, uint8_t *dst) {
    size_t rom_size;

    FILE *file = fopen(pathname, "rb");
    fseek(file, 0, SEEK_END);
    rom_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    fread(dst, sizeof(uint8_t), rom_size, file);
    fclose(file);
}


static inline uint8_t read_register(uint8_t address) {
//    printf("REGISTER READ %02x : %02x\r\n", address, RAM[address]);

    switch (address) {
        // IO
        case 0x00:
        case 0x01:
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
            printf("protection?????????? \r\n");
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


static inline void write_register(uint8_t address, uint8_t  value) {

    switch (address) {
        // TIMER
        case 0x20:
        case 0x21:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
            break;
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
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
            printf("REGISTER WRITE %02x : %02x 0x%04x \r\n", address, value, cpu.pc);
            break;
        default:
            break;
    }

    RAM[address] = value;
}

uint8_t Rd6502(void *, uint16_t address) {
    //printf("rd %04x\n", address);
    if (address <= 0x007F) {
        return read_register(address);
    }

    if (address <= 0x27FF) {
        return RAM[address];
    }

    if (address >= 0x4000 && address <= 0x7FFF) {
        if (address >= 0x7FE2) {
//            printf("IRQ 0x%04x\r\n", address);
        }

        int chip = RAM[0x33] & 1;
        int bank = RAM[0x32] ^ 1;
        address = (address - 0x4000) + bank * 0x4000;
//        printf("bank0 a %04x rom %d  bank %d PC %04x\n", address, chip, RAM[0x32] ^ 1, cpu.PC);
        return chip ? ROM[address] : BIOS[address];
    }

    if (address >= 0x8000) {
        address = (address - 0x8000) +  ((RAM[0x34] & 0x7f) * 0x8000);
        return (RAM[0x34] & 0x80) ? ROM[address] : BIOS[address];
//        return ROM[address - 0x8000];
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

void Wr6502(void *, uint16_t address, uint8_t value) {
    if (address <= 0x007F) {
        write_register(address, value);
        return;
    }
    if (address <= 0x27FF) {
        RAM[address] = value;
        return;
    }

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
            PSG_calc_stereo(&psg, audiobuffer, AUDIO_BUFFER_LENGTH);
            memcpy(currentHeader->lpData, audiobuffer, AUDIO_BUFFER_LENGTH * 2);
            waveOutWrite(hWaveOut, currentHeader, sizeof(WAVEHDR));

            currentHeader++;
            if (currentHeader == waveHeaders + 4) { currentHeader = waveHeaders; }
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    int scale = 4;

    if (!argv[1]) {
        printf("Usage: gamate.exe <rom.bin> [scale_factor]\n");
        exit(-1);
    }

    if (argv[2]) {
        scale = atoi(argv[2]);
    }

    if (!mfb_open("Bit corp. Gamate", 160, 150, scale))
        return 0;

    CreateThread(NULL, 0, SoundThread, NULL, 0, NULL);



    readfile(argv[1], ROM);



    m6502_init(&cpu);

    //cpu.pc= 0xFFFE;
    cpu.read_byte = &Rd6502;
    cpu.write_byte = &Wr6502;
    cpu.m65c02_mode = 1;
    cpu.enable_bcd = 1;
    m6502_gen_res(&cpu);
    printf("PC 0x%04x\n", cpu.pc);

    memset(RAM, 0x00, sizeof(RAM));
    RAM[0x32] = RAM[0x33] = 0;
//    ROM[0x7ffe] = 0xCF;

//    for (int steps = 256; steps--;) {
    for (;;) {
        for (int steps = 256; steps--;) {
            m6502_step(&cpu);
//            m6502_debug_output(&cpu);
        }
        cpu.idf = 1;
        m6502_gen_irq(&cpu, 0x7FE2);
        for (int steps = 256*16; steps--;) {
            m6502_step(&cpu);
//            m6502_debug_output(&cpu);
        }

        cpu.idf = 1;
        m6502_gen_irq(&cpu, 0x7FF4);
        for (int steps = 256*16; steps--;) {
            m6502_step(&cpu);
//            m6502_debug_output(&cpu);
        }
        cpu.idf = 1;
        m6502_gen_irq(&cpu, 0x7FEC);
        m6502_debug_output(&cpu);

        /*
        screen_update((uint16_t *) SCREEN);

        if (mfb_update(SCREEN, 60) == -1)
            exit(1);
        */
    }
}
