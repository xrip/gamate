/* See LICENSE file for license details */

/* Standard library includes */
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#include <windows.h>

#include "MiniFB.h"

#include "m6502/m6502.h"
#include "emu2149/emu2149.h"
#include "gamate/vdp.h"
#include "gamate/bios.h"

static PSG psg;
static M6502 cpu;

static uint8_t RAM[1024] = { 0xFF };
static uint8_t ROM[512 << 10];

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



extern "C" uint8_t Rd6502(uint16_t address) {
    if (address <= 0x1FFF) {
        return RAM[address & 1023];
    }

    if (address >= 0x6000 && address <= 0x9FFF) {
        if (protection < 8) {
            return ((0x47 >> (7 - protection++)) & 1) << 1;
        }

        return ROM[bank0_offset + (address - 0x6000)];
    }

    if (address >= 0xA000 && address <= 0xDFFF) {
        return ROM[bank1_offset + (address - 0xA000)];
    }

    if (address >= 0x5000 && address <= 0x53FF) {
//        if ((address & 7) == 6)
        return vdp_read();
//        exit(1);
    }

    if (address >= 0x5A00 && address <= 0x5AFF) {
        return 0x5B;
    }

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

    if (address == 0x4800) {
        return 0;

    }

// BIOS
    if (address >= 0xE000) {
        return BIOS[address & 4095];
    }

//    printf("READ >>>>>>>>> WTF %04x %04x\r\n", address, m6502_registers.PC);
    return 0xFF;
}

extern "C" void Wr6502(uint16_t address, uint8_t value) {
    if (address <= 0x1FFF) {
        RAM[address & 1023] = value;
        return;
    }

    if (address >= 0x4000 && address <= 0x43FF) {
        PSG_writeReg(&psg, address & 0xf, value);
        return;
    }

    if (address >= 0x5000 && address <= 0x53FF) {
        return vdp_write(address, value);
    }

    if (address >= 0x5900 && address <= 0x59FF) {
        return;
    }

// 4in1 mapper switch first 16kb
    if (address == 0x8000) {
        bank0_offset = 0x4000 * value;
        return;
    }

// regular mapper switch second 16kb
    if (address == 0xC000) {
        bank1_offset = 0x4000 * value;
        return;
    }

//printf("WRITE >>>>>>>>> WTF %04x\r\n", address);
//    exit(1);
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

extern "C" byte Loop6502(M6502 *R) {
    return INT_QUIT;
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

    PSG_init(&psg, 4'433'000 / 4, AUDIO_FREQ);
    PSG_setVolumeMode(&psg, 2); // AY style
    PSG_set_quality(&psg, true);
    PSG_reset(&psg);

    psg.stereo_mask[0] = 0x01;
    psg.stereo_mask[1] = 0x03;
    psg.stereo_mask[2] = 0x02;

    memset(RAM, 0xFF, sizeof(RAM));
    Reset6502(&cpu);
    cpu.IPeriod = 32768;

    for (;;) {
        Run6502(&cpu); Int6502(&cpu, INT_IRQ); // There's a timer that fires
        cpu.IPeriod = 32768;                   // an IRQ every
        Run6502(&cpu); Int6502(&cpu, INT_IRQ); // 32768 clocks (approx. 135.28Hz).

        cpu.IPeriod = 7364;
        Run6502(&cpu);
        cpu.IPeriod = 32768 - 7364;

        screen_update((uint16_t *) SCREEN);

        if (mfb_update(SCREEN, 60) == -1)
            exit(1);
    }
}
