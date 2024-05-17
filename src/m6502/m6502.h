/** M6502: portable 6502 emulator ****************************/
/**                                                         **/
/**                         M6502.h                         **/
/**                                                         **/
/** This file contains declarations relevant to emulation   **/
/** of 6502 CPU.                                            **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1996                      **/
/**               Alex Krasivsky  1996                      **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/
#ifndef M6502_H
#define M6502_H

#ifdef __cplusplus
extern "C" {
#endif

                               /* Loop6502() returns:        */
#define INT_NONE  0            /* No interrupt required      */
#define INT_IRQ   1            /* Standard IRQ interrupt     */
#define INT_NMI   2            /* Non-maskable interrupt     */
#define INT_QUIT  3            /* Exit the emulation         */

                               /* 6502 status flags:         */
#define C_FLAG    0x01         /* 1: Carry occured           */
#define Z_FLAG    0x02         /* 1: Result is zero          */
#define I_FLAG    0x04         /* 1: Interrupts disabled     */
#define D_FLAG    0x08         /* 1: Decimal mode            */
#define B_FLAG    0x10         /* Break [0 on stk after int] */
#define R_FLAG    0x20         /* Always 1                   */
#define V_FLAG    0x40         /* 1: Overflow occured        */
#define N_FLAG    0x80         /* 1: Result is negative      */

/** Simple Datatypes *****************************************/
/** NOTICE: sizeof(byte)=1 and sizeof(word)=2               **/
/*************************************************************/
#ifndef BYTE_TYPE_DEFINED
#define BYTE_TYPE_DEFINED
    typedef unsigned char byte;
#endif
#ifndef WORD_TYPE_DEFINED
#define WORD_TYPE_DEFINED
    typedef unsigned short word;
#endif
typedef signed char offset;

/** Structured Datatypes *************************************/
/*************************************************************/
typedef union
{
#ifdef MSB_FIRST
    struct { byte h, l; } B;
#else
    struct { byte l, h; } B;
#endif
    word W;
} pair;

typedef struct
{
    byte A, P, X, Y, S;  /* CPU registers and program counter   */
    pair PC;

    int IPeriod, ICount; /* Set IPeriod to number of CPU cycles */
                         /* between calls to Loop6502()         */
    byte IRequest;       /* Set to the INT_IRQ when pending IRQ */
    byte AfterCLI;       /* Private, don't touch                */
    int IBackup;         /* Private, don't touch                */
    /* void *User; */    /* Arbitrary user data (ID,RAM*,etc.)  */
} M6502;

/** Reset6502() **********************************************/
/** This function can be used to reset the registers before **/
/** starting execution with Run6502(). It sets registers to **/
/** their initial values.                                   **/
/*************************************************************/
void Reset6502(register M6502 *R);

/** Int6502() ************************************************/
/** This function will generate interrupt of a given type.  **/
/** INT_NMI will cause a non-maskable interrupt. INT_IRQ    **/
/** will cause a normal interrupt, unless I_FLAG set in R.  **/
/*************************************************************/
void Int6502(register M6502 *R, register byte Type);

/** Run6502() ************************************************/
/** This function will run 6502 code until Loop6502() call  **/
/** returns INT_QUIT. It will return the PC at which        **/
/** emulation stopped, and current register values in R.    **/
/*************************************************************/
word Run6502(register M6502 *R);

/** Rd6502()/Wr6502/Op6502() *********************************/
/** These functions are called when access to RAM occurs.   **/
/** They allow to control memory access. Op6502 is the same **/
/** as Rd6502, but used to read *opcodes* only, when many   **/
/** checks can be skipped to make it fast. It is only       **/
/** required if there is a #define FAST_RDOP.               **/
/************************************ TO BE WRITTEN BY USER **/
void Wr6502(register word Addr, register byte Value);
byte Rd6502(register word Addr);
byte Op6502(register word Addr);


/** Loop6502() ***********************************************/
/** 6502 emulation calls this function periodically to      **/
/** check if the system hardware requires any interrupts.   **/
/** This function must return one of following values:      **/
/** INT_NONE, INT_IRQ, INT_NMI, or INT_QUIT to exit the     **/
/** emulation loop.                                         **/
/************************************ TO BE WRITTEN BY USER **/
byte Loop6502(register M6502 *R);

#ifdef __cplusplus
}
#endif

#endif /* M6502_H */
