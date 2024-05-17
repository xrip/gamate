/* SPDX-License-Identifier: MIT */
/* emu2149.h */
#ifndef EMU2149_H_
#define EMU2149_H_
#include "stdint.h"
#define EMU2149_API

#define EMU2149_VOL_DEFAULT 1
#define EMU2149_VOL_YM2149 0
#define EMU2149_VOL_AY_3_8910 1

#define EMU2149_ZX_STEREO			0x80

#define PSG_MASK_CH(x) (1<<(x))

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct __PSG
  {

    /* Volume Table */
    uint32_t *voltbl;

    uint8_t reg[0x20];
    int32_t out;
    int32_t cout[3];

    uint32_t clk, rate, base_incr, quality;

    uint32_t count[3];
    uint32_t volume[3];
    uint32_t freq[3];
    uint32_t edge[3];
    uint32_t tmask[3];
    uint32_t nmask[3];
    uint32_t mask;
    uint32_t stereo_mask[3];

    uint32_t base_count;

    uint32_t env_volume;
    uint32_t env_ptr;
    uint32_t env_face;

    uint32_t env_continue;
    uint32_t env_attack;
    uint32_t env_alternate;
    uint32_t env_hold;
    uint32_t env_pause;
    uint32_t env_reset;

    uint32_t env_freq;
    uint32_t env_count;

    uint32_t noise_seed;
    uint32_t noise_count;
    uint32_t noise_freq;

    /* rate converter */
    uint32_t realstep;
    uint32_t psgtime;
    uint32_t psgstep;
    int32_t prev, next;
    int32_t sprev[2], snext[2];

    /* I/O Ctrl */
    uint32_t adr;

  }
  PSG;

  EMU2149_API void PSG_set_quality (PSG * psg, uint32_t q);
  EMU2149_API void PSG_set_clock(PSG * psg, uint32_t c);
  EMU2149_API void PSG_set_rate (PSG * psg, uint32_t r);
  EMU2149_API void PSG_init (PSG * psg, uint32_t clk, uint32_t rate);
  EMU2149_API void PSG_reset (PSG *);
  EMU2149_API void PSG_delete (PSG *);
  EMU2149_API void PSG_writeReg (PSG *, uint32_t reg, uint32_t val);
  EMU2149_API void PSG_writeIO (PSG * psg, uint32_t adr, uint32_t val);
  EMU2149_API uint8_t PSG_readReg (PSG * psg, uint32_t reg);
  EMU2149_API uint8_t PSG_readIO (PSG * psg);
  EMU2149_API int16_t PSG_calc (PSG *);
  EMU2149_API void PSG_calc_stereo (PSG * psg, int16_t *out, int32_t samples);
  EMU2149_API void PSG_setFlags (PSG * psg, uint8_t flags);
  EMU2149_API void PSG_setVolumeMode (PSG * psg, int type);
  EMU2149_API uint32_t PSG_setMask (PSG *, uint32_t mask);
  EMU2149_API uint32_t PSG_toggleMask (PSG *, uint32_t mask);
  EMU2149_API void PSG_setStereoMask (PSG *psg, uint32_t mask);
    void Pcalc_stereo (PSG * psg, int32_t out[2]);
#ifdef __cplusplus
}
#endif

#endif
