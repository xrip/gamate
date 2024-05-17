/** M6502: portable 6502 emulator ****************************/
/**                                                         **/
/**                          Tables.h                       **/
/**                                                         **/
/** This file contains tables of used by 6502 emulation to  **/
/** compute NEGATIVE and ZERO flags. There are also timing  **/
/** tables for 6502 opcodes. This file is included from     **/
/** 6502.c.                                                 **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1996                      **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

static byte Cycles[256] =
{
/* https://github.com/mamedev/historic-mess/blob/master/src/emu/cpu/m6502/t6502.c
   https://github.com/mamedev/historic-mess/blob/master/src/emu/cpu/m6502/t65c02.c
*/
    7,6,2,2,3,3,5,5,3,2,2,2,2,4,6,5,
    2,5,3,2,3,4,6,5,2,4,2,2,4,4,7,5,
    6,6,2,2,3,3,5,5,4,2,2,2,4,4,6,5,
    2,5,3,2,4,4,6,5,2,4,2,2,4,4,7,5,
    6,6,2,2,2,3,5,5,3,2,2,2,3,4,6,5,
    2,5,3,2,2,4,6,5,2,4,3,2,2,4,7,5,
    6,6,2,2,2,3,5,5,4,2,2,2,5,4,6,5,
    2,5,3,2,4,4,6,5,2,4,4,2,2,4,7,5,
    2,6,2,2,3,3,3,5,2,2,2,2,4,4,4,5,
    2,6,4,2,4,4,4,5,2,5,2,2,4,5,5,5,
    2,6,2,2,3,3,3,5,2,2,2,2,4,4,4,5,
    2,5,3,2,4,4,4,5,2,4,2,2,4,4,4,5,
    2,6,2,2,3,3,5,5,2,2,2,2,4,4,6,5,
    2,5,3,2,2,4,6,5,2,4,3,2,2,4,7,5,
    2,6,2,2,3,3,5,5,2,2,2,2,4,4,6,5,
    2,5,3,2,2,4,6,5,2,4,4,2,2,4,7,5,
};

byte ZNTable[256] =
{
    Z_FLAG,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
    N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
    N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
    N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
    N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
    N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
    N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
    N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
    N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
    N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
    N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
    N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
    N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
    N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
    N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
    N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
};
