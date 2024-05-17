#pragma GCC optimize("Ofast")
/** M65C02: portable 65C02 emulator **************************/
/**                                                         **/
/**                         M65C02.c                        **/
/**                                                         **/
/** This file contains implementation for 65C02 CPU. Don't  **/
/** forget to provide Rd6502(), Wr6502(), Loop6502(), and   **/
/** possibly Op6502() functions to accomodate the emulated  **/
/** machine's architecture.                                 **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1996-2002                 **/
/**               Alex Krasivsky  1996                      **/
/**               Steve Nickolas  2002                      **/
/**   Portions by Holger Picker   2002                      **/
/**   ADC and SBC instructions provided by Scott Hemphill   **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

/* This is M65C02 Version 1.4 of 2002.1220 -uso. */

#include "m6502.h"
#include "tables.h"
#define Op6502(A) Rd6502(A)

/* "Izp" added by uso. */

/** Addressing Methods ***************************************/
/** These macros calculate and return effective addresses.  **/
/*************************************************************/
#define MC_Ab(Rg)       M_LDWORD(Rg)
#define MC_Zp(Rg)       Rg.W=Op6502(R->PC.W++)
#define MC_Zx(Rg)       Rg.W=(byte)(Op6502(R->PC.W++)+R->X)
#define MC_Zy(Rg)       Rg.W=(byte)(Op6502(R->PC.W++)+R->Y)
#define MC_Ax(Rg)       M_LDWORD(Rg);Rg.W+=R->X
#define MC_Ay(Rg)       M_LDWORD(Rg);Rg.W+=R->Y
#define MC_Ix(Rg)       K.W=(byte)(Op6502(R->PC.W++)+R->X); \
                        Rg.B.l=Op6502(K.W++);Rg.B.h=Op6502(K.W)
#define MC_Iy(Rg)       K.W=Op6502(R->PC.W++); \
                        Rg.B.l=Op6502(K.W++);Rg.B.h=Op6502(K.W); \
                        Rg.W+=R->Y
#define MC_Izp(Rg)      K.W=Op6502(R->PC.W++); \
                        Rg.B.l=Op6502(K.W++);Rg.B.h=Op6502(K.W);

/** Reading From Memory **************************************/
/** These macros calculate address and read from it.        **/
/*************************************************************/
#define MR_Ab(Rg)       MC_Ab(J);Rg=Rd6502(J.W)
#define MR_Im(Rg)       Rg=Op6502(R->PC.W++)
#define MR_Zp(Rg)       MC_Zp(J);Rg=Rd6502(J.W)
#define MR_Zx(Rg)       MC_Zx(J);Rg=Rd6502(J.W)
#define MR_Zy(Rg)       MC_Zy(J);Rg=Rd6502(J.W)
#define MR_Ax(Rg)       MC_Ax(J);Rg=Rd6502(J.W)
#define MR_Ay(Rg)       MC_Ay(J);Rg=Rd6502(J.W)
#define MR_Ix(Rg)       MC_Ix(J);Rg=Rd6502(J.W)
#define MR_Iy(Rg)       MC_Iy(J);Rg=Rd6502(J.W)
#define MR_Izp(Rg)      MC_Izp(J);Rg=Rd6502(J.W)

/** Writing To Memory ****************************************/
/** These macros calculate address and write to it.         **/
/*************************************************************/
#define MW_Ab(Rg)       MC_Ab(J);Wr6502(J.W,Rg)
#define MW_Zp(Rg)       MC_Zp(J);Wr6502(J.W,Rg)
#define MW_Zx(Rg)       MC_Zx(J);Wr6502(J.W,Rg)
#define MW_Zy(Rg)       MC_Zy(J);Wr6502(J.W,Rg)
#define MW_Ax(Rg)       MC_Ax(J);Wr6502(J.W,Rg)
#define MW_Ay(Rg)       MC_Ay(J);Wr6502(J.W,Rg)
#define MW_Ix(Rg)       MC_Ix(J);Wr6502(J.W,Rg)
#define MW_Iy(Rg)       MC_Iy(J);Wr6502(J.W,Rg)
#define MW_Izp(Rg)      MC_Izp(J);Wr6502(J.W,Rg)

/** Modifying Memory *****************************************/
/** These macros calculate address and modify it.           **/
/*************************************************************/
#define MM_Ab(Cmd)      MC_Ab(J);I=Rd6502(J.W);Cmd(I);Wr6502(J.W,I)
#define MM_Zp(Cmd)      MC_Zp(J);I=Rd6502(J.W);Cmd(I);Wr6502(J.W,I)
#define MM_Zx(Cmd)      MC_Zx(J);I=Rd6502(J.W);Cmd(I);Wr6502(J.W,I)
#define MM_Ax(Cmd)      MC_Ax(J);I=Rd6502(J.W);Cmd(I);Wr6502(J.W,I)

/** Other Macros *********************************************/
/** Calculating flags, stack, jumps, arithmetics, etc.      **/
/*************************************************************/
#define M_FL(Rg)        R->P=(R->P&~(Z_FLAG|N_FLAG))|ZNTable[Rg]
#define M_LDWORD(Rg)    Rg.B.l=Op6502(R->PC.W++);Rg.B.h=Op6502(R->PC.W++)

#define M_PUSH(Rg)      Wr6502(0x0100|R->S,Rg);R->S--
#define M_POP(Rg)       R->S++;Rg=Op6502(0x0100|R->S)
#define M_JR            R->PC.W+=(offset)Op6502(R->PC.W)+1;R->ICount--

/* Added by uso, fixed by h.p. */
#define M_TSB(Data) R->P = (R->P & ~Z_FLAG) | ((Data & R->A) == 0 ? Z_FLAG : 0);        \
                    Data |=  R->A;
#define M_TRB(Data) R->P = (R->P & ~Z_FLAG) | ((Data & R->A) == 0 ? Z_FLAG : 0);        \
                    Data &= ~R->A;


/* The following code was provided by Mr. Scott Hemphill. Thanks a lot! */
#define M_ADC(Rg)                                                       \
    {                                                                   \
    register unsigned int w;                                            \
    if ((R->A ^ Rg) & 0x80) {                                           \
        R->P &= ~V_FLAG; }                                              \
    else {                                                              \
        R->P |= V_FLAG;  }                                              \
    if (R->P&D_FLAG) {                                                  \
        w = (R->A & 0xf) + (Rg & 0xf) + (R->P & C_FLAG);                \
        if (w >= 10) w = 0x10 | ((w+6)&0xf);                            \
        w += (R->A & 0xf0) + (Rg & 0xf0);                               \
        if (w >= 160) {                                                 \
        R->P |= C_FLAG;                                                 \
        if ((R->P&V_FLAG) && w >= 0x180) R->P &= ~ V_FLAG;              \
        w += 0x60;                                                      \
        } else {                                                        \
        R->P &= ~C_FLAG;                                                \
        if ((R->P&V_FLAG) && w < 0x80) R->P &= ~V_FLAG;                 \
        }                                                               \
    } else {                                                            \
        w = R->A + Rg + (R->P&C_FLAG);                                  \
        if (w >= 0x100) {                                               \
        R->P |= C_FLAG;                                                 \
        if ((R->P & V_FLAG) && w >= 0x180) R->P &= ~V_FLAG;             \
        } else {                                                        \
        R->P &= ~C_FLAG;                                                \
        if ((R->P&V_FLAG) && w < 0x80) R->P &= ~V_FLAG;                 \
        }                                                               \
    }                                                                   \
    R->A = (unsigned char)w;                                            \
    R->P = (R->P & ~(Z_FLAG | N_FLAG)) | (R->A >= 0x80 ? N_FLAG : 0) | (R->A == 0 ? Z_FLAG : 0);    \
    }


#define M_SBC(Rg) SBCInstruction(R, Rg)


#define M_CMP(Rg1,Rg2) \
  K.W=Rg1-Rg2; \
  R->P&=~(N_FLAG|Z_FLAG|C_FLAG); \
  R->P|=ZNTable[K.B.l]|(K.B.h? 0:C_FLAG)
#define M_BIT(Rg) \
  R->P&=~(N_FLAG|V_FLAG|Z_FLAG); \
  R->P|=(Rg&(N_FLAG|V_FLAG))|(Rg&R->A? 0:Z_FLAG)

#define M_AND(Rg)       R->A&=Rg;M_FL(R->A)
#define M_ORA(Rg)       R->A|=Rg;M_FL(R->A)
#define M_EOR(Rg)       R->A^=Rg;M_FL(R->A)
#define M_INC(Rg)       Rg++;M_FL(Rg)
#define M_DEC(Rg)       Rg--;M_FL(Rg)

#define M_ASL(Rg)       R->P&=~C_FLAG;R->P|=Rg>>7;Rg<<=1;M_FL(Rg)
#define M_LSR(Rg)       R->P&=~C_FLAG;R->P|=Rg&C_FLAG;Rg>>=1;M_FL(Rg)
#define M_ROL(Rg)       K.B.l=(Rg<<1)|(R->P&C_FLAG); \
                        R->P&=~C_FLAG;R->P|=Rg>>7;Rg=K.B.l; \
                        M_FL(Rg)
#define M_ROR(Rg)       K.B.l=(Rg>>1)|(R->P<<7); \
                        R->P&=~C_FLAG;R->P|=Rg&C_FLAG;Rg=K.B.l; \
                        M_FL(Rg)

/* The following code was provided by Mr. Scott Hemphill. Thanks a lot again! */
static void SBCInstruction(M6502 *R, register unsigned char val) {
    register unsigned int w;
    register unsigned int temp;

    if ((R->A ^ val) & 0x80) {
        R->P |= V_FLAG;
    }
    else {
        R->P &= ~V_FLAG;
    }

    if (R->P&D_FLAG) {            /* decimal subtraction */
        temp = 0xf + (R->A & 0xf) - (val & 0xf) + (R->P & C_FLAG);
        if (temp < 0x10) {
            w = 0;
            temp -= 6;
        }
        else {
            w = 0x10;
            temp -= 0x10;
        }
        w += 0xf0 + (R->A & 0xf0) - (val & 0xf0);
        if (w < 0x100) {
            R->P &= ~C_FLAG;
            if ((R->P&V_FLAG) && w < 0x80) R->P &= ~V_FLAG;
            w -= 0x60;
        }
        else {
            R->P |= C_FLAG;
            if ((R->P&V_FLAG) && w >= 0x180) R->P &= ~V_FLAG;
        }
        w += temp;
    }
    else {                        /* standard binary subtraction */
        w = 0xff + R->A - val + (R->P&C_FLAG);
        if (w < 0x100) {
            R->P &= ~C_FLAG;
            if ((R->P & V_FLAG) && w < 0x80) R->P &= ~V_FLAG;
        }
        else {
            R->P |= C_FLAG;
            if ((R->P&V_FLAG) && w >= 0x180) R->P &= ~V_FLAG;
        }
    }
    R->A = (unsigned char)w;
    R->P = (R->P & ~(Z_FLAG | N_FLAG)) | (R->A >= 0x80 ? N_FLAG : 0) | (R->A == 0 ? Z_FLAG : 0);
} /* SBCinstruction */

/** Reset6502() **********************************************/
/** This function can be used to reset the registers before **/
/** starting execution with Run6502(). It sets registers to **/
/** their initial values.                                   **/
/*************************************************************/
void Reset6502(M6502 *R)
{
    R->A = R->X = R->Y = 0x00;
    R->P = Z_FLAG | R_FLAG;
    R->S = 0xFF;
    R->PC.B.l = Rd6502(0xFFFC);
    R->PC.B.h = Rd6502(0xFFFD);
    R->ICount = R->IPeriod;
    R->IRequest = INT_NONE;
    R->AfterCLI = 0;
}

/** Int6502() ************************************************/
/** This function will generate interrupt of a given type.  **/
/** INT_NMI will cause a non-maskable interrupt. INT_IRQ    **/
/** will cause a normal interrupt, unless I_FLAG set in R.  **/
/*************************************************************/
void Int6502(M6502 *R, byte Type)
{
    register pair J;

    if ((Type == INT_NMI) || ((Type == INT_IRQ) && !(R->P&I_FLAG)))
    {
        R->ICount -= 7;
        M_PUSH(R->PC.B.h);
        M_PUSH(R->PC.B.l);
        M_PUSH(R->P&~B_FLAG);
        R->P &= ~D_FLAG;
        /* if (R->IAutoReset && (Type == R->IRequest)) R->IRequest = INT_NONE; */
        if (Type == INT_NMI) J.W = 0xFFFA; else { R->P |= I_FLAG; J.W = 0xFFFE; }
        R->PC.B.l = Rd6502(J.W++);
        R->PC.B.h = Rd6502(J.W);
    }
}

/** Run6502() ************************************************/
/** This function will run 6502 code until Loop6502() call  **/
/** returns INT_QUIT. It will return the PC at which        **/
/** emulation stopped, and current register values in R.    **/
/*************************************************************/
word Run6502(M6502 *R)
{
    register pair J, K;
    register byte I;

    for (;;)
    {
        I = Op6502(R->PC.W++);
        R->ICount -= Cycles[I];
        switch (I)
        {
        /* This is M65C02 Version 1.4 of 2002.1220 -uso. */
        case 0x00:                             /* BRK */
            R->PC.W++;
            M_PUSH(R->PC.B.h); M_PUSH(R->PC.B.l);
            M_PUSH(R->P | B_FLAG);
            R->P = (R->P | I_FLAG)&~D_FLAG;
            R->PC.B.l = Rd6502(0xFFFE);
            R->PC.B.h = Rd6502(0xFFFF); break;
        case 0x01: MR_Ix(I); M_ORA(I);  break; /* ORA ($ss,x) INDEXINDIR */

        case 0x04: MM_Zp(M_TSB);        break; /* uso */

        case 0x05: MR_Zp(I); M_ORA(I);  break; /* ORA $ss ZP */
        case 0x06: MM_Zp(M_ASL);        break; /* ASL $ss ZP */
        case 0x08: M_PUSH(R->P);        break; /* PHP */
        case 0x09: MR_Im(I); M_ORA(I);  break; /* ORA #$ss IMM */
        case 0x0A: M_ASL(R->A);         break; /* ASL a ACC */

        case 0x0C: MM_Ab(M_TSB);        break; /* uso */

        case 0x0D: MR_Ab(I); M_ORA(I);  break; /* ORA $ssss ABS */
        case 0x0E: MM_Ab(M_ASL);        break; /* ASL $ssss ABS */
        case 0x10:
            if (R->P&N_FLAG) R->PC.W++;
            else { M_JR; }              break; /* BPL * REL */
        case 0x11: MR_Iy(I); M_ORA(I);  break; /* ORA ($ss),y INDIRINDEX */

        case 0x12: MR_Izp(I); M_ORA(I); break; /* uso */
        case 0x14: MM_Zp(M_TRB);        break; /* uso, FixByME */

        case 0x15: MR_Zx(I); M_ORA(I);  break; /* ORA $ss,x ZP,x */
        case 0x16: MM_Zx(M_ASL);        break; /* ASL $ss,x ZP,x */
        case 0x18: R->P &= ~C_FLAG;     break; /* CLC */
        case 0x19: MR_Ay(I); M_ORA(I);  break; /* ORA $ssss,y ABS,y */

        case 0x1A: M_INC(R->A);         break; /* uso */
        case 0x1C: MM_Ab(M_TRB);        break; /* uso, FixByME */

        case 0x1D: MR_Ax(I); M_ORA(I);  break; /* ORA $ssss,x ABS,x */
        case 0x1E: MM_Ax(M_ASL);        break; /* ASL $ssss,x ABS,x */
        case 0x20:
            K.B.l = Op6502(R->PC.W++);
            K.B.h = Op6502(R->PC.W);
            M_PUSH(R->PC.B.h);
            M_PUSH(R->PC.B.l);
            R->PC = K;                  break;
        case 0x21: MR_Ix(I); M_AND(I);  break; /* AND ($ss,x) INDEXINDIR */
        case 0x24: MR_Zp(I); M_BIT(I);  break; /* BIT $ss ZP */
        case 0x25: MR_Zp(I); M_AND(I);  break; /* AND $ss ZP */
        case 0x26: MM_Zp(M_ROL);        break; /* ROL $ss ZP */
        case 0x28:
            M_POP(I);
            if ((R->IRequest != INT_NONE) && ((I^R->P)&~I&I_FLAG))
            {
                R->AfterCLI = 1;
                R->IBackup = R->ICount;
                R->ICount = 1;
            }
            R->P = I | R_FLAG | B_FLAG; break; /* B_FLAG added from new M6502 */
        case 0x29: MR_Im(I); M_AND(I);  break; /* AND #$ss IMM */
        case 0x2A: M_ROL(R->A);         break; /* ROL a ACC */
        case 0x2C: MR_Ab(I); M_BIT(I);  break; /* BIT $ssss ABS */
        case 0x2D: MR_Ab(I); M_AND(I);  break; /* AND $ssss ABS */
        case 0x2E: MM_Ab(M_ROL);        break; /* ROL $ssss ABS */
        case 0x30:
            if (R->P&N_FLAG) { M_JR; }
            else R->PC.W++;             break; /* BMI * REL */
        case 0x31: MR_Iy(I); M_AND(I);  break;       /* AND ($ss),y INDIRINDEX */

        case 0x32: MR_Izp(I); M_AND(I); break; /* uso */
        case 0x34: MR_Zx(I); M_BIT(I);  break; /* uso */

        case 0x35: MR_Zx(I); M_AND(I);  break; /* AND $ss,x ZP,x */
        case 0x36: MM_Zx(M_ROL);        break; /* ROL $ss,x ZP,x */
        case 0x38: R->P |= C_FLAG;      break; /* SEC */
        case 0x39: MR_Ay(I); M_AND(I);  break; /* AND $ssss,y ABS,y */

        case 0x3A: M_DEC(R->A);         break; /* uso */
        case 0x3C: MR_Ax(I); M_BIT(I);  break; /* uso */

        case 0x3D: MR_Ax(I); M_AND(I);  break; /* AND $ssss,x ABS,x */
        case 0x3E: MM_Ax(M_ROL);        break; /* ROL $ssss,x ABS,x */
        case 0x40:
            M_POP(R->P); R->P |= R_FLAG; M_POP(R->PC.B.l); M_POP(R->PC.B.h); break;
        case 0x41: MR_Ix(I); M_EOR(I);  break; /* EOR ($ss,x) INDEXINDIR */
        case 0x45: MR_Zp(I); M_EOR(I);  break; /* EOR $ss ZP */
        case 0x46: MM_Zp(M_LSR);        break; /* LSR $ss ZP */
        case 0x48: M_PUSH(R->A);        break; /* PHA */
        case 0x49: MR_Im(I); M_EOR(I);  break; /* EOR #$ss IMM */
        case 0x4A: M_LSR(R->A);         break; /* LSR a ACC */
        case 0x4C: M_LDWORD(K); R->PC = K; break;
        case 0x4D: MR_Ab(I); M_EOR(I);  break; /* EOR $ssss ABS */
        case 0x4E: MM_Ab(M_LSR);        break; /* LSR $ssss ABS */
        case 0x50: if (R->P&V_FLAG) R->PC.W++; else { M_JR; } break; /* BVC * REL */
        case 0x51: MR_Iy(I); M_EOR(I);  break; /* EOR ($ss),y INDIRINDEX */
        case 0x52: MR_Izp(I); M_EOR(I); break; /* uso */
        case 0x55: MR_Zx(I); M_EOR(I);  break; /* EOR $ss,x ZP,x */
        case 0x56: MM_Zx(M_LSR);        break; /* LSR $ss,x ZP,x */
        case 0x58: if ((R->IRequest != INT_NONE) && (R->P&I_FLAG))
        {
            R->AfterCLI = 1; R->IBackup = R->ICount; R->ICount = 1;
        }
                   R->P &= ~I_FLAG;     break;
        case 0x59: MR_Ay(I); M_EOR(I);  break; /* EOR $ssss,y ABS,y */
        case 0x5A: M_PUSH(R->Y);        break; /* uso */
        case 0x5D: MR_Ax(I); M_EOR(I);  break; /* EOR $ssss,x ABS,x */
        case 0x5E: MM_Ax(M_LSR);        break; /* LSR $ssss,x ABS,x */
        case 0x60: M_POP(R->PC.B.l); M_POP(R->PC.B.h); R->PC.W++; break;
        case 0x61: MR_Ix(I); M_ADC(I);  break; /* ADC ($ss,x) INDEXINDIR */
        case 0x64: MW_Zp(0);            break; /* uso */
        case 0x65: MR_Zp(I); M_ADC(I);  break; /* ADC $ss ZP */
        case 0x66: MM_Zp(M_ROR);        break; /* ROR $ss ZP */
        case 0x68: M_POP(R->A); M_FL(R->A); break; /* PLA */
        case 0x69: MR_Im(I); M_ADC(I);  break; /* ADC #$ss IMM */
        case 0x6A: M_ROR(R->A);         break; /* ROR a ACC */

        case 0x6C: /* from newer M6502 */
            M_LDWORD(K);
            R->PC.B.l = Rd6502(K.W);
            K.B.l++;
            R->PC.B.h = Rd6502(K.W);    break;
        case 0x6D: MR_Ab(I); M_ADC(I);  break; /* ADC $ssss ABS */
        case 0x6E: MM_Ab(M_ROR);        break; /* ROR $ssss ABS */
        case 0x70: if (R->P&V_FLAG) { M_JR; }
                   else R->PC.W++;      break; /* BVS * REL */
        case 0x71: MR_Iy(I); M_ADC(I);  break; /* ADC ($ss),y INDIRINDEX */
        case 0x72: MR_Izp(I); M_ADC(I); break; /* uso */
        case 0x74: MW_Zx(0);            break; /* uso */
        case 0x75: MR_Zx(I); M_ADC(I);  break; /* ADC $ss,x ZP,x */
        case 0x76: MM_Zx(M_ROR);        break; /* ROR $ss,x ZP,x */
        case 0x78: R->P |= I_FLAG;      break; /* SEI */
        case 0x79: MR_Ay(I); M_ADC(I);  break; /* ADC $ssss,y ABS,y */
        case 0x7A: M_POP(R->Y); M_FL(R->Y); break; /* uso */
        case 0x7C: M_LDWORD(K); R->PC.B.l = Rd6502(K.W++); R->PC.B.h = Rd6502(K.W); R->PC.W += R->X; break; /* uso */
        case 0x7D: MR_Ax(I); M_ADC(I);  break; /* ADC $ssss,x ABS,x */
        case 0x7E: MM_Ax(M_ROR);        break; /* ROR $ssss,x ABS,x */
        case 0x80: M_JR;                break; /* uso */
        case 0x81: MW_Ix(R->A);         break; /* STA ($ss,x) INDEXINDIR */
        case 0x84: MW_Zp(R->Y);         break; /* STY $ss ZP */
        case 0x85: MW_Zp(R->A);         break; /* STA $ss ZP */
        case 0x86: MW_Zp(R->X);         break; /* STX $ss ZP */
        case 0x88: R->Y--; M_FL(R->Y);  break; /* DEY */
        case 0x89: MR_Im(I); M_BIT(I);  break; /* uso */
        case 0x8A: R->A = R->X; M_FL(R->A); break; /* TXA */
        case 0x8C: MW_Ab(R->Y);         break; /* STY $ssss ABS */
        case 0x8D: MW_Ab(R->A);         break; /* STA $ssss ABS */
        case 0x8E: MW_Ab(R->X);         break; /* STX $ssss ABS */
        case 0x90: if (R->P&C_FLAG) R->PC.W++; else { M_JR; } break; /* BCC * REL */
        case 0x91: MW_Iy(R->A);         break; /* STA ($ss),y INDIRINDEX */
        case 0x92: MW_Izp(R->A);        break; /*  uso */
        case 0x94: MW_Zx(R->Y);         break; /* STY $ss,x ZP,x */
        case 0x95: MW_Zx(R->A);         break; /* STA $ss,x ZP,x */
        case 0x96: MW_Zy(R->X);         break; /* STX $ss,y ZP,y */
        case 0x98: R->A = R->Y; M_FL(R->A); break; /* TYA */
        case 0x99: MW_Ay(R->A);         break; /* STA $ssss,y ABS,y */
        case 0x9A: R->S = R->X;         break; /* TXS */
        case 0x9C: MW_Ab(0);            break; /* uso */
        case 0x9D: MW_Ax(R->A);         break; /* STA $ssss,x ABS,x */
        case 0x9E: MW_Ax(0);            break; /* uso */
        case 0xA0: MR_Im(R->Y); M_FL(R->Y); break; /* LDY #$ss IMM */
        case 0xA1: MR_Ix(R->A); M_FL(R->A); break; /* LDA ($ss,x) INDEXINDIR */
        case 0xA2: MR_Im(R->X); M_FL(R->X); break; /* LDX #$ss IMM */
        case 0xA4: MR_Zp(R->Y); M_FL(R->Y); break; /* LDY $ss ZP */
        case 0xA5: MR_Zp(R->A); M_FL(R->A); break; /* LDA $ss ZP */
        case 0xA6: MR_Zp(R->X); M_FL(R->X); break; /* LDX $ss ZP */
        case 0xA8: R->Y = R->A; M_FL(R->Y); break; /* TAY */
        case 0xA9: MR_Im(R->A); M_FL(R->A); break; /* LDA #$ss IMM */
        case 0xAA: R->X = R->A; M_FL(R->X); break; /* TAX */
        case 0xAC: MR_Ab(R->Y); M_FL(R->Y); break; /* LDY $ssss ABS */
        case 0xAD: MR_Ab(R->A); M_FL(R->A); break; /* LDA $ssss ABS */
        case 0xAE: MR_Ab(R->X); M_FL(R->X); break; /* LDX $ssss ABS */
        case 0xB0: if (R->P&C_FLAG) { M_JR; }
                   else R->PC.W++;          break; /* BCS * REL */
        case 0xB1: MR_Iy(R->A); M_FL(R->A); break; /* LDA ($ss),y INDIRINDEX */
        case 0xB2: MR_Izp(R->A); M_FL(R->A); break; /* uso */
        case 0xB4: MR_Zx(R->Y); M_FL(R->Y); break; /* LDY $ss,x ZP,x */
        case 0xB5: MR_Zx(R->A); M_FL(R->A); break; /* LDA $ss,x ZP,x */
        case 0xB6: MR_Zy(R->X); M_FL(R->X); break; /* LDX $ss,y ZP,y */
        case 0xB8: R->P &= ~V_FLAG;         break; /* CLV */
        case 0xB9: MR_Ay(R->A); M_FL(R->A); break; /* LDA $ssss,y ABS,y */
        case 0xBA: R->X = R->S; M_FL(R->X); break; /* TSX */
        case 0xBC: MR_Ax(R->Y); M_FL(R->Y); break; /* LDY $ssss,x ABS,x */
        case 0xBD: MR_Ax(R->A); M_FL(R->A); break; /* LDA $ssss,x ABS,x */
        case 0xBE: MR_Ay(R->X); M_FL(R->X); break; /* LDX $ssss,y ABS,y */
        case 0xC0: MR_Im(I); M_CMP(R->Y, I); break; /* CPY #$ss IMM */
        case 0xC1: MR_Ix(I); M_CMP(R->A, I); break; /* CMP ($ss,x) INDEXINDIR */
        case 0xC4: MR_Zp(I); M_CMP(R->Y, I); break; /* CPY $ss ZP */
        case 0xC5: MR_Zp(I); M_CMP(R->A, I); break; /* CMP $ss ZP */
        case 0xC6: MM_Zp(M_DEC);             break; /* DEC $ss ZP */
        case 0xC8: R->Y++; M_FL(R->Y);       break; /* INY */
        case 0xC9: MR_Im(I); M_CMP(R->A, I); break; /* CMP #$ss IMM */
        case 0xCA: R->X--; M_FL(R->X);       break; /* DEX */
        case 0xCC: MR_Ab(I); M_CMP(R->Y, I); break; /* CPY $ssss ABS */
        case 0xCD: MR_Ab(I); M_CMP(R->A, I); break; /* CMP $ssss ABS */
        case 0xCE: MM_Ab(M_DEC);             break; /* DEC $ssss ABS */
        case 0xD0: if (R->P&Z_FLAG) R->PC.W++; else { M_JR; } break; /* BNE * REL */
        case 0xD1: MR_Iy(I); M_CMP(R->A, I); break; /* CMP ($ss),y INDIRINDEX */
        case 0xD2: MR_Izp(I); M_CMP(R->A, I); break; /* uso */
        case 0xD5: MR_Zx(I); M_CMP(R->A, I); break; /* CMP $ss,x ZP,x */
        case 0xD6: MM_Zx(M_DEC);             break; /* DEC $ss,x ZP,x */
        case 0xD8: R->P &= ~D_FLAG;          break; /* CLD */
        case 0xD9: MR_Ay(I); M_CMP(R->A, I); break; /* CMP $ssss,y ABS,y */
        case 0xDA: M_PUSH(R->X);             break; /* uso */
        case 0xDD: MR_Ax(I); M_CMP(R->A, I); break; /* CMP $ssss,x ABS,x */
        case 0xDE: MM_Ax(M_DEC);             break; /* DEC $ssss,x ABS,x */
        case 0xE0: MR_Im(I); M_CMP(R->X, I); break; /* CPX #$ss IMM */
        case 0xE1: MR_Ix(I); M_SBC(I);       break; /* SBC ($ss,x) INDEXINDIR */
        case 0xE4: MR_Zp(I); M_CMP(R->X, I); break; /* CPX $ss ZP */
        case 0xE5: MR_Zp(I); M_SBC(I);       break; /* SBC $ss ZP */
        case 0xE6: MM_Zp(M_INC);             break; /* INC $ss ZP */
        case 0xE8: R->X++; M_FL(R->X);       break; /* INX */
        case 0xE9: MR_Im(I); M_SBC(I);       break; /* SBC #$ss IMM */
        case 0xEA:                           break; /* NOP */
        case 0xEC: MR_Ab(I); M_CMP(R->X, I); break; /* CPX $ssss ABS */
        case 0xED: MR_Ab(I); M_SBC(I);       break; /* SBC $ssss ABS */
        case 0xEE: MM_Ab(M_INC);             break; /* INC $ssss ABS */

        case 0xF0: if (R->P&Z_FLAG) { M_JR; }
                   else R->PC.W++;           break; /* BEQ * REL */
        case 0xF1: MR_Iy(I); M_SBC(I);       break; /* SBC ($ss),y INDIRINDEX */
        case 0xF2: MR_Izp(I); M_SBC(I);      break; /* uso */
        case 0xF5: MR_Zx(I); M_SBC(I);       break; /* SBC $ss,x ZP,x */
        case 0xF6: MM_Zx(M_INC);             break; /* INC $ss,x ZP,x */
        case 0xF8: R->P |= D_FLAG;           break; /* SED */
        case 0xF9: MR_Ay(I); M_SBC(I);       break; /* SBC $ssss,y ABS,y */
        case 0xFA: M_POP(R->X); M_FL(R->X);  break; /* uso */
        case 0xFD: MR_Ax(I); M_SBC(I);       break; /* SBC $ssss,x ABS,x */
        case 0xFE: MM_Ax(M_INC);             break; /* INC $ssss,x ABS,x */
        default:
            break;
        }

        /* If cycle counter expired... */
        if (R->ICount <= 0)
        {
            /* If we have come after CLI, get INT_? from IRequest */
            /* Otherwise, get it from the loop handler            */
            if (R->AfterCLI)
            {
                I = R->IRequest;             /* Get pending interrupt     */
                R->ICount += R->IBackup - 1; /* Restore the ICount        */
                R->AfterCLI = 0;             /* Done with AfterCLI state  */
            }
            else
            {
                I = Loop6502(R);         /* Call the periodic handler */
                R->ICount = R->IPeriod;  /* Reset the cycle counter   */
                /* if (!I) I = R->IRequest; */ /* Realize pending interrupt */
            }

            if (I == INT_QUIT) return(R->PC.W); /* Exit if INT_QUIT     */
            if (I) Int6502(R, I);               /* Interrupt if needed  */
        }
    }

    /* Execution stopped */
    return(R->PC.W);
}
