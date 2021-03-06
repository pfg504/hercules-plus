/* CPU.C        (c) Copyright Roger Bowler, 1994-2011                */
/*              ESA/390 CPU Emulator                                 */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/* Interpretive Execution - (c) Copyright Jan Jaeger, 1999-2011      */
/* z/Architecture support - (c) Copyright Jan Jaeger, 1999-2011      */

// $Id$

/*-------------------------------------------------------------------*/
/* This module implements the CPU instruction execution function of  */
/* the S/370 and ESA/390 architectures, as described in the manuals  */
/* GA22-7000-03 System/370 Principles of Operation                   */
/* SA22-7201-06 ESA/390 Principles of Operation                      */
/* SA22-7832-00 z/Architecture Principles of Operation               */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Additional credits:                                               */
/*      Nullification corrections by Jan Jaeger                      */
/*      Set priority by Reed H. Petty from an idea by Steve Gay      */
/*      Corrections to program check by Jan Jaeger                   */
/*      Light optimization on critical path by Valery Pogonchenko    */
/*      OSTAILOR parameter by Jay Maynard                            */
/*      CPU timer and clock comparator interrupt improvements by     */
/*          Jan Jaeger, after a suggestion by Willem Konynenberg     */
/*      Instruction decode rework - Jan Jaeger                       */
/*      Modifications for Interpretive Execution (SIE) by Jan Jaeger */
/*      Basic FP extensions support - Peter Kuschnerus           v209*/
/*      ASN-and-LX-reuse facility - Roger Bowler, June 2004      @ALR*/
/*-------------------------------------------------------------------*/

#include "hstdinc.h"

#if !defined(_HENGINE_DLL_)
#define _HENGINE_DLL_
#endif

#if !defined(_CPU_C_)
#define _CPU_C_
#endif

#include "hercules.h"
#include "opcode.h"
#include "inline.h"

/*-------------------------------------------------------------------*/
/* Put a CPU in check-stop state                                     */
/* Must hold the system intlock                                      */
/*-------------------------------------------------------------------*/
void ARCH_DEP(checkstop_cpu)(REGS *regs)
{
    regs->cpustate=CPUSTATE_STOPPING;
    regs->checkstop=1;
    ON_IC_INTERRUPT(regs);
}
/*-------------------------------------------------------------------*/
/* Put all the CPUs in the configuration in check-stop state         */
/*-------------------------------------------------------------------*/
void ARCH_DEP(checkstop_config)(void)
{
    int i;
    for(i=0;i<sysblk.maxcpu;i++)
    {
        if(IS_CPU_ONLINE(i))
        {
            ARCH_DEP(checkstop_cpu)(sysblk.regs[i]);
        }
    }
    WAKEUP_CPUS_MASK(sysblk.waiting_mask);
}

/*-------------------------------------------------------------------*/
/* Store current PSW at a specified address in main storage          */
/*-------------------------------------------------------------------*/
void ARCH_DEP(store_psw) (REGS *regs, BYTE *addr)
{

    /* Ensure psw.IA is set */
    if (!regs->psw.zeroilc)
        SET_PSW_IA(regs);

#if defined(FEATURE_BCMODE)
    if ( ECMODE(&regs->psw) ) {
#endif /*defined(FEATURE_BCMODE)*/
#if !defined(FEATURE_ESAME)
        STORE_FW ( addr,
                   ( (regs->psw.sysmask << 24)
                   | ((regs->psw.pkey | regs->psw.states) << 16)
                   | ( ( (regs->psw.asc)
                       | (regs->psw.cc << 4)
                       | (regs->psw.progmask)
                       ) << 8
                     )
                   | regs->psw.zerobyte
                   )
                 );
        if(unlikely(regs->psw.zeroilc))
            STORE_FW ( addr + 4, regs->psw.IA | (regs->psw.amode ? 0x80000000 : 0) );
        else
            STORE_FW ( addr + 4,
                   ( (regs->psw.IA & ADDRESS_MAXWRAP(regs)) | (regs->psw.amode ? 0x80000000 : 0) )
                 );
#endif /*!defined(FEATURE_ESAME)*/
#if defined(FEATURE_BCMODE)
    } else {
        STORE_FW ( addr,
                   ( (regs->psw.sysmask << 24)
                   | ((regs->psw.pkey | regs->psw.states) << 16)
                   | (regs->psw.intcode)
                   )
                 );
        if(unlikely(regs->psw.zeroilc))
            STORE_FW ( addr + 4,
                   ( ( (REAL_ILC(regs) << 5)
                     | (regs->psw.cc << 4)
                     | regs->psw.progmask
                     ) << 24
                   ) | regs->psw.IA
                 );
        else
            STORE_FW ( addr + 4,
                   ( ( (REAL_ILC(regs) << 5)
                     | (regs->psw.cc << 4)
                     | regs->psw.progmask
                     ) << 24
                   ) | (regs->psw.IA & ADDRESS_MAXWRAP(regs))
                 );
    }
#elif defined(FEATURE_ESAME)
        STORE_FW ( addr,
                   ( (regs->psw.sysmask << 24)
                   | ((regs->psw.pkey | regs->psw.states) << 16)
                   | ( ( (regs->psw.asc)
                       | (regs->psw.cc << 4)
                       | (regs->psw.progmask)
                       ) << 8
                     )
                   | (regs->psw.amode64 ? 0x01 : 0)
                   | regs->psw.zerobyte
                   )
                 );
        STORE_FW ( addr + 4,
                   ( (regs->psw.amode ? 0x80000000 : 0 )
                   | regs->psw.zeroword
                   )
                 );
        STORE_DW ( addr + 8, regs->psw.IA_G );
#endif /*defined(FEATURE_ESAME)*/
} /* end function ARCH_DEP(store_psw) */

/*-------------------------------------------------------------------*/
/* Load current PSW from a specified address in main storage         */
/* Returns 0 if valid, 0x0006 if specification exception             */
/*-------------------------------------------------------------------*/
int ARCH_DEP(load_psw) (REGS *regs, BYTE *addr)
{
    INVALIDATE_AIA(regs);

    regs->psw.zeroilc = 1;

    regs->psw.sysmask = addr[0];
    regs->psw.pkey    = (addr[1] & 0xF0);
    regs->psw.states  = (addr[1] & 0x0F);

#if defined(FEATURE_BCMODE)
    if ( ECMODE(&regs->psw) ) {
#endif /*defined(FEATURE_BCMODE)*/

        SET_IC_ECMODE_MASK(regs);

#if defined(FEATURE_S380)
        /* In case we are running CMS under CP, this LPSW may
           be wiping out the 31-bit flag. So record the address
           ready to be restored. */
        if ( sysblk.s380 
             && regs->psw.amode 
             && (regs->psw.s380_bc == 0
                 || regs->cr[1].F.L.F == 0) )
        {
            regs->psw.s380_bc = regs->psw.IA_L;
            regs->psw.s380_cr1 = regs->cr[1].F.L.F;
        }
#endif

        /* Processing for EC mode PSW */
        regs->psw.intcode  = 0;
        regs->psw.asc      = (addr[2] & 0xC0);
        regs->psw.cc       = (addr[2] & 0x30) >> 4;
        regs->psw.progmask = (addr[2] & 0x0F);
        regs->psw.amode    = (addr[4] & 0x80) ? 1 : 0;

#if defined(FEATURE_ESAME)
        regs->psw.zerobyte = addr[3] & 0xFE;
        regs->psw.amode64  = addr[3] & 0x01;
        regs->psw.zeroword = fetch_fw(addr+4) & 0x7FFFFFFF;
        regs->psw.IA       = fetch_dw (addr + 8);
        regs->psw.AMASK    = regs->psw.amode64 ? AMASK64
                           : regs->psw.amode   ? AMASK31 : AMASK24;
#else /*!defined(FEATURE_ESAME)*/
        regs->psw.zerobyte = addr[3];
        regs->psw.amode64  = 0;
        regs->psw.IA       = fetch_fw(addr + 4) & 0x7FFFFFFF;
#if defined(FEATURE_S380)
        /* Because when running CMS under CP there are LPSWs
           being done in virtual BCMODE that don't know about
           AMODE 31, we need to restore the app's AMODE
           status by looking at the address */
        if (sysblk.s380 
            && (regs->psw.s380_bc != 0) 
            && (regs->psw.IA_L == regs->psw.s380_bc)
            && (regs->cr[1].F.L.F == regs->psw.s380_cr1)) {
            regs->psw.s380_bc = 0;
            regs->psw.s380_cr1 = 0;
            regs->psw.amode = 1;
        }
#endif
        regs->psw.AMASK    = regs->psw.amode ? AMASK31 : AMASK24;
#endif /*!defined(FEATURE_ESAME)*/

        /* Bits 0 and 2-4 of system mask must be zero */
        if ((addr[0] & 0xB8) != 0)
            return PGM_SPECIFICATION_EXCEPTION;

#if defined(FEATURE_ESAME)
        /* For ESAME, bit 12 must be zero */
        if (NOTESAME(&regs->psw))
            return PGM_SPECIFICATION_EXCEPTION;

        /* Bits 24-30 must be zero */
        if (regs->psw.zerobyte)
            return PGM_SPECIFICATION_EXCEPTION;

        /* Bits 33-63 must be zero */
        if ( regs->psw.zeroword )
            return PGM_SPECIFICATION_EXCEPTION;
#else /*!defined(FEATURE_ESAME)*/
        /* Bits 24-31 must be zero */
        if ( regs->psw.zerobyte )
            return PGM_SPECIFICATION_EXCEPTION;

        /* For ESA/390, bit 12 must be one */
        if (!ECMODE(&regs->psw))
            return PGM_SPECIFICATION_EXCEPTION;
#endif /*!defined(FEATURE_ESAME)*/

#ifndef FEATURE_DUAL_ADDRESS_SPACE
        /* If DAS feature not installed then bit 16 must be zero */
        if (SPACE_BIT(&regs->psw))
            return PGM_SPECIFICATION_EXCEPTION;
#endif /*!FEATURE_DUAL_ADDRESS_SPACE*/

#ifndef FEATURE_ACCESS_REGISTERS
        /* If not ESA/370 or ESA/390 then bit 17 must be zero */
        if (AR_BIT(&regs->psw))
            return PGM_SPECIFICATION_EXCEPTION;
#endif /*!FEATURE_ACCESS_REGISTERS*/

        /* Check validity of amode and instruction address */
#if defined(FEATURE_ESAME)
        /* For ESAME, bit 32 cannot be zero if bit 31 is one */
        if (regs->psw.amode64 && !regs->psw.amode)
            return PGM_SPECIFICATION_EXCEPTION;

        /* If bit 32 is zero then IA cannot exceed 24 bits */
        if (!regs->psw.amode && regs->psw.IA > 0x00FFFFFF)
            return PGM_SPECIFICATION_EXCEPTION;

        /* If bit 31 is zero then IA cannot exceed 31 bits */
        if (!regs->psw.amode64 && regs->psw.IA > 0x7FFFFFFF)
            return PGM_SPECIFICATION_EXCEPTION;
#else /*!defined(FEATURE_ESAME)*/
  #ifdef FEATURE_BIMODAL_ADDRESSING
        /* For 370-XA, ESA/370, and ESA/390,
           if amode=24, bits 33-39 must be zero */
        if (!regs->psw.amode && regs->psw.IA > 0x00FFFFFF)
            return PGM_SPECIFICATION_EXCEPTION;
  #else /*!FEATURE_BIMODAL_ADDRESSING*/
        /* For S/370, bits 32-39 must be zero */
        if (addr[4] != 0x00)
            return PGM_SPECIFICATION_EXCEPTION;
  #endif /*!FEATURE_BIMODAL_ADDRESSING*/
#endif /*!defined(FEATURE_ESAME)*/

#if defined(FEATURE_BCMODE)
    } else {

        SET_IC_BCMODE_MASK(regs);

        /* Processing for S/370 BC mode PSW */
        regs->psw.intcode = fetch_hw (addr + 2);
        regs->psw.cc = (addr[4] & 0x30) >> 4;
        regs->psw.progmask = (addr[4] & 0x0F);

        FETCH_FW(regs->psw.IA, addr + 4);
        regs->psw.IA &= 0x00FFFFFF;
        regs->psw.AMASK = AMASK24;

        regs->psw.zerobyte = 0;
        regs->psw.asc = 0;
        regs->psw.amode64 = regs->psw.amode = 0;
    }
#endif /*defined(FEATURE_BCMODE)*/

#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
    /* Bits 5 and 16 must be zero in XC mode */
    if( SIE_STATB(regs, MX, XC)
      && ( (regs->psw.sysmask & PSW_DATMODE) || SPACE_BIT(&regs->psw)) )
        return PGM_SPECIFICATION_EXCEPTION;
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/

    regs->psw.zeroilc = 0;

    /* Check for wait state PSW */
    if (WAITSTATE(&regs->psw) && CPU_STEPPING_OR_TRACING_ALL)
    {
        char buf[40];
        WRMSG(HHC00800, "I", PTYPSTR(regs->cpuad), regs->cpuad, str_psw(regs, buf));
    }

    TEST_SET_AEA_MODE(regs);

    return 0;
} /* end function ARCH_DEP(load_psw) */

/*-------------------------------------------------------------------*/
/* Load program interrupt new PSW                                    */
/*-------------------------------------------------------------------*/
void (ATTR_REGPARM(2) ARCH_DEP(program_interrupt)) (REGS *regs, int pcode)
{
PSA    *psa;                            /* -> Prefixed storage area  */
REGS   *realregs;                       /* True regs structure       */
RADR    px;                             /* host real address of pfx  */
int     code;                           /* pcode without PER ind.    */
int     ilc;                            /* instruction length        */
#if defined(FEATURE_ESAME)
/** FIXME : SEE ISW20090110-1 */
void   *zmoncode=NULL;                  /* special reloc for z/Arch  */
                 /* FIXME : zmoncode not being initialized here raises
                    a potentially non-initialized warning in GCC..
                    can't find why. ISW 2009/02/04 */
                                        /* mon call SIE intercept    */
#endif
#if defined(FEATURE_INTERPRETIVE_EXECUTION)
int     sie_ilc=0;                      /* SIE instruction length    */
#endif
#if defined(_FEATURE_SIE)
int     nointercept;                    /* True for virtual pgmint   */
#endif /*defined(_FEATURE_SIE)*/
#if defined(OPTION_FOOTPRINT_BUFFER)
U32     n;
#endif /*defined(OPTION_FOOTPRINT_BUFFER)*/
char    dxcstr[8]={0};                  /* " DXC=xx" if data excptn  */

static char *pgmintname[] = {
        /* 01 */        "Operation exception",
        /* 02 */        "Privileged-operation exception",
        /* 03 */        "Execute exception",
        /* 04 */        "Protection exception",
        /* 05 */        "Addressing exception",
        /* 06 */        "Specification exception",
        /* 07 */        "Data exception",
        /* 08 */        "Fixed-point-overflow exception",
        /* 09 */        "Fixed-point-divide exception",
        /* 0A */        "Decimal-overflow exception",
        /* 0B */        "Decimal-divide exception",
        /* 0C */        "HFP-exponent-overflow exception",
        /* 0D */        "HFP-exponent-underflow exception",
        /* 0E */        "HFP-significance exception",
        /* 0F */        "HFP-floating-point-divide exception",
        /* 10 */        "Segment-translation exception",
        /* 11 */        "Page-translation exception",
        /* 12 */        "Translation-specification exception",
        /* 13 */        "Special-operation exception",
        /* 14 */        "Pseudo-page-fault exception",
        /* 15 */        "Operand exception",
        /* 16 */        "Trace-table exception",
        /* 17 */        "ASN-translation exception",
        /* 18 */        "Page access exception",
        /* 19 */        "Vector/Crypto operation exception",
        /* 1A */        "Page state exception",
        /* 1B */        "Page transition exception",
        /* 1C */        "Space-switch event",
        /* 1D */        "Square-root exception",
        /* 1E */        "Unnormalized-operand exception",
        /* 1F */        "PC-translation specification exception",
        /* 20 */        "AFX-translation exception",
        /* 21 */        "ASX-translation exception",
        /* 22 */        "LX-translation exception",
        /* 23 */        "EX-translation exception",
        /* 24 */        "Primary-authority exception",
        /* 25 */        "Secondary-authority exception",
        /* 26 */        "LFX-translation exception",            /*@ALR*/
        /* 27 */        "LSX-translation exception",            /*@ALR*/
        /* 28 */        "ALET-specification exception",
        /* 29 */        "ALEN-translation exception",
        /* 2A */        "ALE-sequence exception",
        /* 2B */        "ASTE-validity exception",
        /* 2C */        "ASTE-sequence exception",
        /* 2D */        "Extended-authority exception",
        /* 2E */        "LSTE-sequence exception",              /*@ALR*/
        /* 2F */        "ASTE-instance exception",              /*@ALR*/
        /* 30 */        "Stack-full exception",
        /* 31 */        "Stack-empty exception",
        /* 32 */        "Stack-specification exception",
        /* 33 */        "Stack-type exception",
        /* 34 */        "Stack-operation exception",
        /* 35 */        "Unassigned exception",
        /* 36 */        "Unassigned exception",
        /* 37 */        "Unassigned exception",
        /* 38 */        "ASCE-type exception",
        /* 39 */        "Region-first-translation exception",
        /* 3A */        "Region-second-translation exception",
        /* 3B */        "Region-third-translation exception",
        /* 3C */        "Unassigned exception",
        /* 3D */        "Unassigned exception",
        /* 3E */        "Unassigned exception",
        /* 3F */        "Unassigned exception",
        /* 40 */        "Monitor event" };

        /* 26 */ /* was "Page-fault-assist exception", */
        /* 27 */ /* was "Control-switch exception", */

    /* If called with ghost registers (ie from hercules command
       then ignore all interrupt handling and report the error
       to the caller */
    if(regs->ghostregs)
        longjmp(regs->progjmp, pcode);

    PTT(PTT_CL_PGM,"*PROG",pcode,(U32)(regs->TEA & 0xffffffff),regs->psw.IA_L);

    /* program_interrupt() may be called with a shadow copy of the
       regs structure, realregs is the pointer to the real structure
       which must be used when loading/storing the psw, or backing up
       the instruction address in case of nullification */
#if defined(_FEATURE_SIE)
        realregs = SIE_MODE(regs)
                 ? sysblk.regs[regs->cpuad]->guestregs
                 : sysblk.regs[regs->cpuad];
#else /*!defined(_FEATURE_SIE)*/
    realregs = sysblk.regs[regs->cpuad];
#endif /*!defined(_FEATURE_SIE)*/

    /* Prevent machine check when in (almost) interrupt loop */
    realregs->instcount++;

    /* Release any locks */
    if (sysblk.intowner == realregs->cpuad)
        RELEASE_INTLOCK(realregs);
    if (sysblk.mainowner == realregs->cpuad)
        RELEASE_MAINLOCK(realregs);

    /* Ensure psw.IA is set and aia invalidated */
    INVALIDATE_AIA(realregs);
#if defined(FEATURE_INTERPRETIVE_EXECUTION)
    if(realregs->sie_active)
        INVALIDATE_AIA(realregs->guestregs);
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/

    /* Set instruction length (ilc) */
    ilc = realregs->psw.zeroilc ? 0 : REAL_ILC(realregs);
    if (realregs->psw.ilc == 0 && !realregs->psw.zeroilc)
    {
        /* This can happen if BALR, BASR, BASSM or BSM
           program checks during trace */
        ilc = likely(!realregs->execflag) ? 2 : realregs->exrl ? 6 : 4;
        realregs->ip += ilc;
        realregs->psw.IA += ilc;
        realregs->psw.ilc = ilc;
    }
#if defined(FEATURE_INTERPRETIVE_EXECUTION)
    if(realregs->sie_active)
    {
        sie_ilc = realregs->guestregs->psw.zeroilc ? 0 :
                REAL_ILC(realregs->guestregs);
        if (realregs->guestregs->psw.ilc == 0
         && !realregs->guestregs->psw.zeroilc)
        {
            sie_ilc = likely(!realregs->guestregs->execflag) ? 2 :
                    realregs->guestregs->exrl ? 6 : 4;
            realregs->guestregs->psw.ilc = sie_ilc;
        }
    }
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/

    /* Set `execflag' to 0 in case EXecuted instruction program-checked */
    realregs->execflag = 0;
#if defined(FEATURE_INTERPRETIVE_EXECUTION)
    if(realregs->sie_active)
        realregs->guestregs->execflag = 0;
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/

    /* Unlock the main storage lock if held */
    if (realregs->cpuad == sysblk.mainowner)
        RELEASE_MAINLOCK(realregs);

    /* Remove PER indication from program interrupt code
       such that interrupt code specific tests may be done.
       The PER indication will be stored in the PER handling
       code */
    code = pcode & ~PGM_PER_EVENT;

    /* If this is a concurrent PER event then we must add the PER
       bit to the interrupts code */
    if( OPEN_IC_PER(realregs) )
        pcode |= PGM_PER_EVENT;

    /* Perform serialization and checkpoint synchronization */
    PERFORM_SERIALIZATION (realregs);
    PERFORM_CHKPT_SYNC (realregs);

#if defined(FEATURE_INTERPRETIVE_EXECUTION)
    /* Host protection and addressing exceptions must be
       reflected to the guest */
    if(realregs->sie_active &&
        (code == PGM_PROTECTION_EXCEPTION
      || code == PGM_ADDRESSING_EXCEPTION
#if defined(_FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
      || code == PGM_ALET_SPECIFICATION_EXCEPTION
      || code == PGM_ALEN_TRANSLATION_EXCEPTION
      || code == PGM_ALE_SEQUENCE_EXCEPTION
      || code == PGM_EXTENDED_AUTHORITY_EXCEPTION
#endif /*defined(_FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/
        ) )
    {
#if defined(SIE_DEBUG)
        logmsg(_("program_int() passing to guest code=%4.4X\n"),pcode);
#endif /*defined(SIE_DEBUG)*/
        realregs->guestregs->TEA = realregs->TEA;
        realregs->guestregs->excarid = realregs->excarid;
        realregs->guestregs->opndrid = realregs->opndrid;
#if defined(_FEATURE_PROTECTION_INTERCEPTION_CONTROL)
        realregs->guestregs->hostint = 1;
#endif /*defined(_FEATURE_PROTECTION_INTERCEPTION_CONTROL)*/
        (realregs->guestregs->program_interrupt) (realregs->guestregs, pcode);
    }
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/

    /* Back up the PSW for exceptions which cause nullification,
       unless the exception occurred during instruction fetch */
    if ((code == PGM_PAGE_TRANSLATION_EXCEPTION
      || code == PGM_SEGMENT_TRANSLATION_EXCEPTION
#if defined(FEATURE_ESAME)
      || code == PGM_ASCE_TYPE_EXCEPTION
      || code == PGM_REGION_FIRST_TRANSLATION_EXCEPTION
      || code == PGM_REGION_SECOND_TRANSLATION_EXCEPTION
      || code == PGM_REGION_THIRD_TRANSLATION_EXCEPTION
#endif /*defined(FEATURE_ESAME)*/
      || code == PGM_TRACE_TABLE_EXCEPTION
      || code == PGM_AFX_TRANSLATION_EXCEPTION
      || code == PGM_ASX_TRANSLATION_EXCEPTION
      || code == PGM_LX_TRANSLATION_EXCEPTION
      || code == PGM_LFX_TRANSLATION_EXCEPTION                 /*@ALR*/
      || code == PGM_LSX_TRANSLATION_EXCEPTION                 /*@ALR*/
      || code == PGM_LSTE_SEQUENCE_EXCEPTION                   /*@ALR*/
      || code == PGM_EX_TRANSLATION_EXCEPTION
      || code == PGM_PRIMARY_AUTHORITY_EXCEPTION
      || code == PGM_SECONDARY_AUTHORITY_EXCEPTION
      || code == PGM_ALEN_TRANSLATION_EXCEPTION
      || code == PGM_ALE_SEQUENCE_EXCEPTION
      || code == PGM_ASTE_VALIDITY_EXCEPTION
      || code == PGM_ASTE_SEQUENCE_EXCEPTION
      || code == PGM_ASTE_INSTANCE_EXCEPTION                   /*@ALR*/
      || code == PGM_EXTENDED_AUTHORITY_EXCEPTION
      || code == PGM_STACK_FULL_EXCEPTION
      || code == PGM_STACK_EMPTY_EXCEPTION
      || code == PGM_STACK_SPECIFICATION_EXCEPTION
      || code == PGM_STACK_TYPE_EXCEPTION
      || code == PGM_STACK_OPERATION_EXCEPTION
      || code == PGM_VECTOR_OPERATION_EXCEPTION)
      && !realregs->instinvalid)
    {
        realregs->psw.IA -= ilc;
        realregs->psw.IA &= ADDRESS_MAXWRAP(realregs);
#if defined(FEATURE_INTERPRETIVE_EXECUTION)
        /* When in SIE mode the guest instruction causing this
           host exception must also be nullified */
        if(realregs->sie_active && !realregs->guestregs->instinvalid)
        {
            realregs->guestregs->psw.IA -= sie_ilc;
            realregs->guestregs->psw.IA &= ADDRESS_MAXWRAP(realregs->guestregs);
        }
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/
    }

    /* The OLD PSW must be incremented on the following
       exceptions during instfetch */
    if(realregs->instinvalid &&
      (  code == PGM_PROTECTION_EXCEPTION
      || code == PGM_ADDRESSING_EXCEPTION
      || code == PGM_SPECIFICATION_EXCEPTION
      || code == PGM_TRANSLATION_SPECIFICATION_EXCEPTION ))
    {
        realregs->psw.IA += ilc;
        realregs->psw.IA &= ADDRESS_MAXWRAP(realregs);
    }

    /* Store the interrupt code in the PSW */
    realregs->psw.intcode = pcode;

    /* Call debugger if active */
    HDC2(debug_program_interrupt, regs, pcode);

    /* Trace program checks other then PER event */
    if(code && (CPU_STEPPING_OR_TRACING(realregs, ilc)
        || sysblk.pgminttr & ((U64)1 << ((code - 1) & 0x3F))))
    {
     BYTE *ip;
     char buf1[10] = "";
     char buf2[32] = "";
#if defined(OPTION_FOOTPRINT_BUFFER)
        if(!(sysblk.insttrace || sysblk.inststep))
            for(n = sysblk.footprptr[realregs->cpuad] + 1 ;
                n != sysblk.footprptr[realregs->cpuad];
                n++, n &= OPTION_FOOTPRINT_BUFFER - 1)
                ARCH_DEP(display_inst)
                        (&sysblk.footprregs[realregs->cpuad][n],
                        sysblk.footprregs[realregs->cpuad][n].inst);
#endif /*defined(OPTION_FOOTPRINT_BUFFER)*/
#if defined(_FEATURE_SIE)
        if (SIE_MODE(realregs))
          strlcpy(buf1, "SIE: ", sizeof(buf1) );
#endif /*defined(_FEATURE_SIE)*/
#if defined(SIE_DEBUG)
       strlcpy(buf2, QSTR(_GEN_ARCH), sizeof(buf2) );
       strlcat(buf2, " ", sizeof(buf2) );
#endif /*defined(SIE_DEBUG)*/
       if (code == PGM_DATA_EXCEPTION)
           snprintf(dxcstr, sizeof(dxcstr), " DXC=%2.2X", regs->dxc);
       dxcstr[sizeof(dxcstr)-1] = '\0';
       WRMSG(HHC00801, "I",
        PTYPSTR(realregs->cpuad), realregs->cpuad, buf1, buf2,
                pgmintname[ (code - 1) & 0x3F], pcode, ilc, dxcstr);

        /* Calculate instruction pointer */
        ip = realregs->instinvalid ? NULL
           : (realregs->ip - ilc < realregs->aip)
             ? realregs->inst : realregs->ip - ilc;

        ARCH_DEP(display_inst) (realregs, ip);
    }

    realregs->instinvalid = 0;

#if defined(FEATURE_INTERPRETIVE_EXECUTION)
    /* If this is a host exception in SIE state then leave SIE */
    if(realregs->sie_active)
        ARCH_DEP(sie_exit) (realregs, SIE_HOST_PGMINT);
#endif /*defined(FEATURE_INTERPRETIVE_EXECUTION)*/

    /* Absolute address of prefix page */
    px = realregs->PX;

    /* If under SIE use translated to host absolute prefix */
#if defined(_FEATURE_SIE)
    if(SIE_MODE(regs))
        px = regs->sie_px;
#endif

#if defined(_FEATURE_SIE)
    if(!SIE_MODE(regs) ||
      /* Interception is mandatory for the following exceptions */
      (
#if defined(_FEATURE_PROTECTION_INTERCEPTION_CONTROL)
         !(code == PGM_PROTECTION_EXCEPTION
           && (!SIE_FEATB(regs, EC2, PROTEX)
             || realregs->hostint))
#else /*!defined(_FEATURE_PROTECTION_INTERCEPTION_CONTROL)*/
         code != PGM_PROTECTION_EXCEPTION
#endif /*!defined(_FEATURE_PROTECTION_INTERCEPTION_CONTROL)*/
#if defined (_FEATURE_PER2)
      && !((pcode & PGM_PER_EVENT) && SIE_FEATB(regs, M, GPE))
#endif /* defined (_FEATURE_PER2) */
      && code != PGM_ADDRESSING_EXCEPTION
      && code != PGM_SPECIFICATION_EXCEPTION
      && code != PGM_SPECIAL_OPERATION_EXCEPTION
#ifdef FEATURE_VECTOR_FACILITY
      && code != PGM_VECTOR_OPERATION_EXCEPTION
#endif /*FEATURE_VECTOR_FACILITY*/
#if defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)
      && !(code == PGM_ALEN_TRANSLATION_EXCEPTION
        && SIE_FEATB(regs, MX, XC))
      && !(code == PGM_ALE_SEQUENCE_EXCEPTION
        && SIE_FEATB(regs, MX, XC))
      && !(code == PGM_EXTENDED_AUTHORITY_EXCEPTION
        && SIE_FEATB(regs, MX, XC))
#endif /*defined(FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE)*/
      /* And conditional for the following exceptions */
      && !(code == PGM_OPERATION_EXCEPTION
        && SIE_FEATB(regs, IC0, OPEREX))
      && !(code == PGM_PRIVILEGED_OPERATION_EXCEPTION
        && SIE_FEATB(regs, IC0, PRIVOP))
#ifdef FEATURE_BASIC_FP_EXTENSIONS
      && !(code == PGM_DATA_EXCEPTION
        && (regs->dxc == 1 || regs->dxc == 2)
        && (regs->CR(0) & CR0_AFP)
        && !(regs->hostregs->CR(0) & CR0_AFP))
#endif /*FEATURE_BASIC_FP_EXTENSIONS*/
      /* Or all exceptions if requested as such */
      && !SIE_FEATB(regs, IC0, PGMALL) )
    )
    {
#endif /*defined(_FEATURE_SIE)*/
        /* Set the main storage reference and change bits */
        STORAGE_KEY(px, regs) |= (STORKEY_REF | STORKEY_CHANGE);

        /* Point to PSA in main storage */
        psa = (void*)(regs->mainstor + px);
#if defined(_FEATURE_SIE)
#if defined(FEATURE_ESAME)
/** FIXME : SEE ISW20090110-1 */
        if(code == PGM_MONITOR_EVENT)
        {
            zmoncode=psa->moncode;
        }
#endif
        nointercept = 1;
    }
    else
    {
        /* This is a guest interruption interception so point to
           the interruption parm area in the state descriptor
           rather then the PSA, except for the operation exception */
        if(code != PGM_OPERATION_EXCEPTION)
        {
            psa = (void*)(regs->hostregs->mainstor + SIE_STATE(regs) + SIE_IP_PSA_OFFSET);
            /* Set the main storage reference and change bits */
            STORAGE_KEY(SIE_STATE(regs), regs->hostregs) |= (STORKEY_REF | STORKEY_CHANGE);
#if defined(FEATURE_ESAME)
/** FIXME : SEE ISW20090110-1 */
            if(code == PGM_MONITOR_EVENT)
            {
                PSA *_psa;
                _psa=(void *)(regs->hostregs->mainstor + SIE_STATE(regs) + SIE_II_PSA_OFFSET);
                zmoncode=_psa->ioid;
            }
#endif
        }
        else
        {
            /* Point to PSA in main storage */
            psa = (void*)(regs->mainstor + px);

            /* Set the main storage reference and change bits */
            STORAGE_KEY(px, regs) |= (STORKEY_REF | STORKEY_CHANGE);
        }

        nointercept = 0;
    }
#endif /*defined(_FEATURE_SIE)*/

#if defined(_FEATURE_PER)
    /* Handle PER or concurrent PER event */

    /* Throw out Stor Alter PER if merged with nullified/suppressed rupt */
    if ( IS_IC_PER_SA(realregs) && !IS_IC_PER_STURA(realregs) &&
                                   (realregs->ip[0] != 0x0E) &&
         !(code == 0x00 || code == 0x06 || code == 0x08 || code == 0x0A ||
           code == 0x0C || code == 0x0D || code == 0x0E || code == 0x1C ||
           code == 0x40) )
              OFF_IC_PER_SA(realregs);

    if( OPEN_IC_PER(realregs) )
    {
        if( CPU_STEPPING_OR_TRACING(realregs, ilc) )
            WRMSG(HHC00802, "I", PTYPSTR(regs->cpuad), regs->cpuad,
              pcode, IS_IC_PER(realregs) >> 16,
              (realregs->psw.IA - ilc) & ADDRESS_MAXWRAP(realregs) );

        realregs->perc |= OPEN_IC_PER(realregs) >> ((32 - IC_CR9_SHIFT) - 16);

        /* Positions 14 and 15 contain zeros if a storage alteration
           event was not indicated */
//FIXME: is this right??
        if( !(OPEN_IC_PER_SA(realregs))
          || (OPEN_IC_PER_STURA(realregs)) )
            realregs->perc &= 0xFFFC;

        STORE_HW(psa->perint, realregs->perc);

        STORE_W(psa->peradr, realregs->peradr);

        if( IS_IC_PER_SA(realregs) && ACCESS_REGISTER_MODE(&realregs->psw) )
            psa->perarid = realregs->peraid;

#if defined(_FEATURE_SIE)
        /* Reset PER pending indication */
        if(nointercept)
            OFF_IC_PER(realregs);
#endif
    }
    else
    {
        pcode &= 0xFF7F;
    }
#endif /*defined(_FEATURE_PER)*/


#if defined(FEATURE_BCMODE)
    /* For ECMODE, store extended interrupt information in PSA */
    if ( ECMODE(&realregs->psw) )
#endif /*defined(FEATURE_BCMODE)*/
    {
        /* Store the program interrupt code at PSA+X'8C' */
        psa->pgmint[0] = 0;
        psa->pgmint[1] = ilc;
        STORE_HW(psa->pgmint + 2, pcode);

        /* Store the exception access identification at PSA+160 */
        if ( code == PGM_PAGE_TRANSLATION_EXCEPTION
          || code == PGM_SEGMENT_TRANSLATION_EXCEPTION
#if defined(FEATURE_ESAME)
          || code == PGM_ASCE_TYPE_EXCEPTION
          || code == PGM_REGION_FIRST_TRANSLATION_EXCEPTION
          || code == PGM_REGION_SECOND_TRANSLATION_EXCEPTION
          || code == PGM_REGION_THIRD_TRANSLATION_EXCEPTION
#endif /*defined(FEATURE_ESAME)*/
          || code == PGM_ALEN_TRANSLATION_EXCEPTION
          || code == PGM_ALE_SEQUENCE_EXCEPTION
          || code == PGM_ASTE_VALIDITY_EXCEPTION
          || code == PGM_ASTE_SEQUENCE_EXCEPTION
          || code == PGM_ASTE_INSTANCE_EXCEPTION               /*@ALR*/
          || code == PGM_EXTENDED_AUTHORITY_EXCEPTION
#ifdef FEATURE_SUPPRESSION_ON_PROTECTION
          || code == PGM_PROTECTION_EXCEPTION
#endif /*FEATURE_SUPPRESSION_ON_PROTECTION*/
           )
        {
            psa->excarid = regs->excarid;
            if(regs->TEA | TEA_MVPG)
                psa->opndrid = regs->opndrid;
            realregs->opndrid = 0;
        }

#if defined(FEATURE_ESAME)
        /* Store the translation exception address at PSA+168 */
        if ( code == PGM_PAGE_TRANSLATION_EXCEPTION
          || code == PGM_SEGMENT_TRANSLATION_EXCEPTION
          || code == PGM_ASCE_TYPE_EXCEPTION
          || code == PGM_REGION_FIRST_TRANSLATION_EXCEPTION
          || code == PGM_REGION_SECOND_TRANSLATION_EXCEPTION
          || code == PGM_REGION_THIRD_TRANSLATION_EXCEPTION
#ifdef FEATURE_SUPPRESSION_ON_PROTECTION
          || code == PGM_PROTECTION_EXCEPTION
#endif /*FEATURE_SUPPRESSION_ON_PROTECTION*/
           )
        {
            STORE_DW(psa->TEA_G, regs->TEA);
        }

        /* Store the translation exception address at PSA+172 */
        if ( code == PGM_AFX_TRANSLATION_EXCEPTION
          || code == PGM_ASX_TRANSLATION_EXCEPTION
          || code == PGM_PRIMARY_AUTHORITY_EXCEPTION
          || code == PGM_SECONDARY_AUTHORITY_EXCEPTION
          || code == PGM_SPACE_SWITCH_EVENT
          || code == PGM_LX_TRANSLATION_EXCEPTION
          || code == PGM_LFX_TRANSLATION_EXCEPTION             /*@ALR*/
          || code == PGM_LSX_TRANSLATION_EXCEPTION             /*@ALR*/
          || code == PGM_LSTE_SEQUENCE_EXCEPTION               /*@ALR*/
          || code == PGM_EX_TRANSLATION_EXCEPTION)
        {
            STORE_FW(psa->TEA_L, regs->TEA);
        }
#else /*!defined(FEATURE_ESAME)*/
        /* Store the translation exception address at PSA+144 */
        if ( code == PGM_PAGE_TRANSLATION_EXCEPTION
          || code == PGM_SEGMENT_TRANSLATION_EXCEPTION
          || code == PGM_AFX_TRANSLATION_EXCEPTION
          || code == PGM_ASX_TRANSLATION_EXCEPTION
          || code == PGM_PRIMARY_AUTHORITY_EXCEPTION
          || code == PGM_SECONDARY_AUTHORITY_EXCEPTION
          || code == PGM_SPACE_SWITCH_EVENT
          || code == PGM_LX_TRANSLATION_EXCEPTION
          || code == PGM_EX_TRANSLATION_EXCEPTION
#ifdef FEATURE_SUPPRESSION_ON_PROTECTION
          || code == PGM_PROTECTION_EXCEPTION
#endif /*FEATURE_SUPPRESSION_ON_PROTECTION*/
           )
        {
            STORE_FW(psa->tea, regs->TEA);
        }
#endif /*!defined(FEATURE_ESAME)*/
        realregs->TEA = 0;

        /* Store Data exception code in PSA */
        if (code == PGM_DATA_EXCEPTION)
        {
            STORE_FW(psa->DXC, regs->dxc);
#ifdef FEATURE_BASIC_FP_EXTENSIONS
            /* Load data exception code into FPC register byte 2 */
            if(regs->CR(0) & CR0_AFP)
            {
                regs->fpc &= ~(FPC_DXC);
                regs->fpc |= ((regs->dxc << 8)) & FPC_DXC;
            }
#endif /*FEATURE_BASIC_FP_EXTENSIONS*/
        }

        /* Store the monitor class and event code */
        if (code == PGM_MONITOR_EVENT)
        {
            STORE_HW(psa->monclass, regs->monclass);

            /* Store the monitor code word at PSA+156 */
            /* or doubleword at PSA+176               */
            /* ISW20090110-1 ZSIEMCFIX                */
            /* In the event of a z/Arch guest being   */
            /* intercepted during a succesful Monitor */
            /* call, the monitor code is not stored   */
            /* at psa->moncode (which is beyond sie2bk->ip */
            /* but rather at the same location as an  */
            /* I/O interrupt would store the SSID     */
            /*    zmoncode points to this location    */
            /*  **** FIXME **** FIXME  *** FIXME ***  */
            /* ---- The determination of the location */
            /*      of the z/Sie Intercept moncode    */
            /*      should be made more flexible      */
            /*      and should be put somewhere in    */
            /*      esa390.h                          */
            /*  **** FIXME **** FIXME  *** FIXME ***  */
#if defined(FEATURE_ESAME)
            STORE_DW(zmoncode, regs->MONCODE);
#else
            STORE_W(psa->moncode, regs->MONCODE);
#endif
        }

#if defined(FEATURE_PER3)
        /* Store the breaking event address register in the PSA */
        SET_BEAR_REG(regs, regs->bear_ip);
        STORE_W(psa->bea, regs->bear);
#endif /*defined(FEATURE_PER3)*/

    } /* end if(ECMODE) */

#if defined(_FEATURE_PROTECTION_INTERCEPTION_CONTROL)
    realregs->hostint = 0;
#endif /*defined(_FEATURE_PROTECTION_INTERCEPTION_CONTROL)*/

#if defined(_FEATURE_SIE)
    if(nointercept)
    {
#endif /*defined(_FEATURE_SIE)*/
//FIXME: Why are we getting intlock here??
//      OBTAIN_INTLOCK(realregs);

        /* Store current PSW at PSA+X'28' or PSA+X'150' for ESAME */
        ARCH_DEP(store_psw) (realregs, psa->pgmold);

        /* Load new PSW from PSA+X'68' or PSA+X'1D0' for ESAME */
        if ( (code = ARCH_DEP(load_psw) (realregs, psa->pgmnew)) )
        {
#if defined(_FEATURE_SIE)
            if(SIE_MODE(realregs))
            {
//              RELEASE_INTLOCK(realregs);
                longjmp(realregs->progjmp, pcode);
            }
            else
#endif /*defined(_FEATURE_SIE)*/
            {
                char buf[64];

                WRMSG(HHC00803, "I", PTYPSTR(realregs->cpuad), realregs->cpuad,
                         str_psw (realregs, buf));
                OBTAIN_INTLOCK(realregs);
                realregs->cpustate = CPUSTATE_STOPPING;
                ON_IC_INTERRUPT(realregs);
                RELEASE_INTLOCK(realregs);
            }
        }

//      RELEASE_INTLOCK(realregs);

        longjmp(realregs->progjmp, SIE_NO_INTERCEPT);

#if defined(_FEATURE_SIE)
    }

    longjmp (realregs->progjmp, pcode);
#endif /*defined(_FEATURE_SIE)*/

} /* end function ARCH_DEP(program_interrupt) */

/*-------------------------------------------------------------------*/
/* Load restart new PSW                                              */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(restart_interrupt) (REGS *regs)
{
int     rc;                             /* Return code               */
PSA    *psa;                            /* -> Prefixed storage area  */

    PTT(PTT_CL_INF,"*RESTART",regs->cpuad,regs->cpustate,regs->psw.IA_L);

    /* Set the main storage reference and change bits */
    STORAGE_KEY(regs->PX, regs) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Zeroize the interrupt code in the PSW */
    regs->psw.intcode = 0;

    /* Point to PSA in main storage */
    psa = (PSA*)(regs->mainstor + regs->PX);

    /* Store current PSW at PSA+X'8' or PSA+X'120' for ESAME  */
    ARCH_DEP(store_psw) (regs, psa->RSTOLD);

    /* Load new PSW from PSA+X'0' or PSA+X'1A0' for ESAME */
    rc = ARCH_DEP(load_psw) (regs, psa->RSTNEW);

    if ( rc == 0)
    {
        regs->opinterv = 0;
        regs->cpustate = CPUSTATE_STARTED;
    }

    RELEASE_INTLOCK(regs);

    if ( rc )
        regs->program_interrupt(regs, rc);

    longjmp (regs->progjmp, SIE_INTERCEPT_RESTART);
} /* end function restart_interrupt */


/*-------------------------------------------------------------------*/
/* Perform I/O interrupt if pending                                  */
/* Note: The caller MUST hold the interrupt lock (sysblk.intlock)    */
/*-------------------------------------------------------------------*/
void ARCH_DEP(perform_io_interrupt) (REGS *regs)
{
int     rc;                             /* Return code               */
int     icode;                          /* Intercept code            */
PSA    *psa;                            /* -> Prefixed storage area  */
U32     ioparm;                         /* I/O interruption parameter*/
U32     ioid;                           /* I/O interruption address  */
U32     iointid;                        /* I/O interruption ident    */
RADR    pfx;                            /* Prefix                    */
DBLWRD  csw;                            /* CSW for S/370 channels    */

    /* Test and clear pending I/O interrupt */
    icode = ARCH_DEP(present_io_interrupt) (regs, &ioid, &ioparm, &iointid, csw);

    /* Exit if no interrupt was presented */
    if (icode == 0) return;

    PTT(PTT_CL_IO,"*IOINT",ioid,ioparm,iointid);

#if defined(_FEATURE_IO_ASSIST)
    if(SIE_MODE(regs) && icode != SIE_NO_INTERCEPT)
    {
        /* Point to SIE copy of PSA in state descriptor */
        psa = (void*)(regs->hostregs->mainstor + SIE_STATE(regs) + SIE_II_PSA_OFFSET);
        STORAGE_KEY(SIE_STATE(regs), regs->hostregs) |= (STORKEY_REF | STORKEY_CHANGE);
    }
    else
#endif
    {
        /* Point to PSA in main storage */
        pfx =
#if defined(_FEATURE_SIE)
              SIE_MODE(regs) ? regs->sie_px :
#endif
              regs->PX;
        psa = (void*)(regs->mainstor + pfx);
        STORAGE_KEY(pfx, regs) |= (STORKEY_REF | STORKEY_CHANGE);
    }

#ifdef FEATURE_S370_CHANNEL
    /* Store the channel status word at PSA+X'40' */
    memcpy (psa->csw, csw, 8);

    /* Set the interrupt code to the I/O device address */
    regs->psw.intcode = ioid;

    /* For ECMODE, store the I/O device address at PSA+X'B8' */
    if (ECMODE(&regs->psw))
        STORE_FW(psa->ioid, ioid);

    /* Trace the I/O interrupt */
    if (CPU_STEPPING_OR_TRACING(regs, 0))
        WRMSG (HHC00804, "I", PTYPSTR(regs->cpuad), regs->cpuad,
                regs->psw.intcode,
                csw[0], csw[1], csw[2], csw[3],
                csw[4], csw[5], csw[6], csw[7]);
#endif /*FEATURE_S370_CHANNEL*/

#ifdef FEATURE_CHANNEL_SUBSYSTEM
    /* Store X'0001' + subchannel number at PSA+X'B8' */
    STORE_FW(psa->ioid, ioid);

    /* Store the I/O interruption parameter at PSA+X'BC' */
    STORE_FW(psa->ioparm, ioparm);

#if defined(FEATURE_ESAME) || defined(_FEATURE_IO_ASSIST)
    /* Store the I/O interruption identification word at PSA+X'C0' */
    STORE_FW(psa->iointid, iointid);
#endif /*defined(FEATURE_ESAME)*/

    /* Trace the I/O interrupt */
    if (CPU_STEPPING_OR_TRACING(regs, 0))
#if !defined(FEATURE_ESAME) && !defined(_FEATURE_IO_ASSIST)
        WRMSG (HHC00805, "I", PTYPSTR(regs->cpuad), regs->cpuad, ioid, ioparm);
#else /*defined(FEATURE_ESAME)*/
        WRMSG (HHC00806, "I", PTYPSTR(regs->cpuad), regs->cpuad, ioid, ioparm, iointid);
#endif /*defined(FEATURE_ESAME)*/
#endif /*FEATURE_CHANNEL_SUBSYSTEM*/

#if defined(_FEATURE_IO_ASSIST)
    if(icode == SIE_NO_INTERCEPT)
#endif
    {
        /* Store current PSW at PSA+X'38' or PSA+X'170' for ESAME */
        ARCH_DEP(store_psw) ( regs, psa->iopold );

        /* Load new PSW from PSA+X'78' or PSA+X'1F0' for ESAME */
        rc = ARCH_DEP(load_psw) ( regs, psa->iopnew );

        if ( rc )
        {
            RELEASE_INTLOCK(regs);
            regs->program_interrupt (regs, rc);
        }
    }

    RELEASE_INTLOCK(regs);

    longjmp(regs->progjmp, icode);

} /* end function perform_io_interrupt */

/*-------------------------------------------------------------------*/
/* Perform Machine Check interrupt if pending                        */
/* Note: The caller MUST hold the interrupt lock (sysblk.intlock)    */
/*-------------------------------------------------------------------*/
static void ARCH_DEP(perform_mck_interrupt) (REGS *regs)
{
int     rc;                             /* Return code               */
PSA    *psa;                            /* -> Prefixed storage area  */
U64     mcic;                           /* Mach.check interrupt code */
U32     xdmg;                           /* External damage code      */
RADR    fsta;                           /* Failing storage address   */

    /* Test and clear pending machine check interrupt */
    rc = ARCH_DEP(present_mck_interrupt) (regs, &mcic, &xdmg, &fsta);

    /* Exit if no machine check was presented */
    if (rc == 0) return;

    /* Set the main storage reference and change bits */
    STORAGE_KEY(regs->PX, regs) |= (STORKEY_REF | STORKEY_CHANGE);

    /* Point to the PSA in main storage */
    psa = (void*)(regs->mainstor + regs->PX);

    /* Store registers in machine check save area */
    ARCH_DEP(store_status) (regs, regs->PX);

#if !defined(FEATURE_ESAME)
// ZZ
    /* Set the extended logout area to zeros */
    memset(psa->storepsw, 0, 16);
#endif

    /* Store the machine check interrupt code at PSA+232 */
    STORE_DW(psa->mckint, mcic);

    /* Trace the machine check interrupt */
    if (CPU_STEPPING_OR_TRACING(regs, 0))
        WRMSG (HHC00807, "I", PTYPSTR(regs->cpuad), regs->cpuad, mcic);

    /* Store the external damage code at PSA+244 */
    STORE_FW(psa->xdmgcode, xdmg);

#if defined(FEATURE_ESAME)
    /* Store the failing storage address at PSA+248 */
    STORE_DW(psa->mcstorad, fsta);
#else /*!defined(FEATURE_ESAME)*/
    /* Store the failing storage address at PSA+248 */
    STORE_FW(psa->mcstorad, fsta);
#endif /*!defined(FEATURE_ESAME)*/

    /* Store current PSW at PSA+X'30' */
    ARCH_DEP(store_psw) ( regs, psa->mckold );

    /* Load new PSW from PSA+X'70' */
    rc = ARCH_DEP(load_psw) ( regs, psa->mcknew );

    RELEASE_INTLOCK(regs);

    if ( rc )
        regs->program_interrupt (regs, rc);

    longjmp (regs->progjmp, SIE_INTERCEPT_MCK);
} /* end function perform_mck_interrupt */


#if !defined(_GEN_ARCH)


REGS *s370_run_cpu (int cpu, REGS *oldregs);
REGS *s390_run_cpu (int cpu, REGS *oldregs);
REGS *z900_run_cpu (int cpu, REGS *oldregs);
static REGS *(* run_cpu[GEN_MAXARCH]) (int cpu, REGS *oldregs) =
                {
#if defined(_370)
                    s370_run_cpu,
#endif
#if defined(_390)
                    s390_run_cpu,
#endif
#if defined(_900)
                    z900_run_cpu
#endif
                };


/*-------------------------------------------------------------------*/
/* CPU instruction execution thread                                  */
/*-------------------------------------------------------------------*/
void *cpu_thread (int *ptr)
{
REGS *regs = NULL;
int   cpu  = *ptr;
char  cpustr[40];
int   rc;

#if defined(USE_GETTID)
    sysblk.cputidp[cpu] = gettid();
#endif /*defined(USE_GETTID)*/

    OBTAIN_INTLOCK(NULL);

    /* Increment number of CPUs online */
    sysblk.cpus++;

    /* Set hi CPU */
    if (cpu >= sysblk.hicpu)
        sysblk.hicpu = cpu + 1;

    /* Start the TOD clock and CPU timer thread */
    if (!sysblk.todtid)
    {
        rc = create_thread (&sysblk.todtid, DETACHED,
             timer_update_thread, NULL, "timer_update_thread");
        if (rc)
        {
            WRMSG(HHC00102, "E", strerror(rc));
            RELEASE_INTLOCK(NULL);
            return NULL;
        }
    }
    /* Set root mode in order to set priority */
    SETMODE(ROOT);

    /* Set CPU thread priority */
    if (setpriority(PRIO_PROCESS, 0, sysblk.cpuprio))
        WRMSG(HHC00136, "W", "setpriority()", strerror(errno));

    /* Back to user mode */
    SETMODE(USER);

    /* Display thread started message on control panel */
    MSGBUF( cpustr, "Processor %s%02X", PTYPSTR( cpu ), cpu );
    WRMSG(HHC00100, "I", (u_long)thread_id(), getpriority(PRIO_PROCESS,0), cpustr);

    /* Execute the program in specified mode */
    do {
        regs = run_cpu[sysblk.arch_mode] (cpu, regs);
    } while (regs);

    /* Decrement number of CPUs online */
    sysblk.cpus--;

    /* Reset hi cpu */
    if (cpu + 1 >= sysblk.hicpu)
    {
        int i;
        for (i = sysblk.maxcpu - 1; i >= 0; i--)
            if (IS_CPU_ONLINE(i))
                break;
        sysblk.hicpu = i + 1;
    }

    /* Signal cpu has terminated */
    signal_condition (&sysblk.cpucond);

    /* Display thread ended message on control panel */
    WRMSG(HHC00101, "I", (u_long)thread_id(),  getpriority(PRIO_PROCESS,0), cpustr);

    RELEASE_INTLOCK(NULL);

    return NULL;
}


/*-------------------------------------------------------------------*/
/* Initialize a CPU                                                  */
/*-------------------------------------------------------------------*/
int cpu_init (int cpu, REGS *regs, REGS *hostregs)
{
int i;

    obtain_lock (&sysblk.cpulock[cpu]);

    /* initialize eye-catchers */
    memset(&regs->blknam,SPACE,sizeof(regs->blknam));
    memset(&regs->blkver,SPACE,sizeof(regs->blkver));
    memset(&regs->blkend,SPACE,sizeof(regs->blkend));
    regs->blkloc = swap_byte_U64((U64)((uintptr_t)regs));
    regs->blksiz = swap_byte_U32((U32)sizeof(REGS));
    {
        char cputyp[32];
        char buf[32];

        MSGBUF( cputyp, "%-4.4s_%s%02X", HDL_NAME_REGS, PTYPSTR( cpu ), cpu );

        memcpy(regs->blknam,cputyp,strlen(cputyp)>sizeof(regs->blknam) ? sizeof(regs->blknam) : strlen(cputyp) );
        memcpy(regs->blkver,HDL_VERS_REGS,strlen(HDL_VERS_REGS));

        MSGBUF( buf, "END%13.13s", cputyp );
        memcpy(regs->blkend, buf, strlen(buf)>sizeof(regs->blkend) ? sizeof(regs->blkend): strlen(buf) );
    }


    regs->cpuad = cpu;
    regs->cpubit = CPU_BIT(cpu);
    regs->arch_mode = sysblk.arch_mode;
    regs->mainstor = sysblk.mainstor;
    regs->sysblk = &sysblk;
    /*
     * ISW20060125 : LINE REMOVED : This is the job of
     *               the INITIAL CPU RESET
     */
#if 0
    regs->psa = (PSA*)regs->mainstor;
#endif
    regs->storkeys = sysblk.storkeys;
    regs->mainlim = sysblk.mainsize - 1;
    regs->tod_epoch = get_tod_epoch();

    initialize_condition (&regs->intcond);
    regs->cpulock = &sysblk.cpulock[cpu];

#if defined(_FEATURE_VECTOR_FACILITY)
    regs->vf = &sysblk.vf[cpu];
    regs->vf->online = (cpu < sysblk.numvec);
#endif /*defined(_FEATURE_VECTOR_FACILITY)*/
    initial_cpu_reset(regs);

    if (hostregs == NULL)
    {
        regs->cpustate = CPUSTATE_STOPPING;
        ON_IC_INTERRUPT(regs);
        regs->hostregs = regs;
        regs->host = 1;
        sysblk.regs[cpu] = regs;
        sysblk.config_mask |= regs->cpubit;
        sysblk.started_mask |= regs->cpubit;
    }
    else
    {
        hostregs->guestregs = regs;
        regs->hostregs = hostregs;
        regs->guestregs = regs;
        regs->guest = 1;
        regs->sie_mode = 1;
        regs->opinterv = 0;
        regs->cpustate = CPUSTATE_STARTED;
    }

    /* Initialize accelerated lookup fields */
    regs->CR_G(CR_ASD_REAL) = TLB_REAL_ASD;

    for(i = 0; i < 16; i++)
        regs->AEA_AR(i)               = CR_ASD_REAL;
    regs->AEA_AR(USE_INST_SPACE)      = CR_ASD_REAL;
    regs->AEA_AR(USE_REAL_ADDR)       = CR_ASD_REAL;
    regs->AEA_AR(USE_PRIMARY_SPACE)   =  1;
    regs->AEA_AR(USE_SECONDARY_SPACE) =  7;
    regs->AEA_AR(USE_HOME_SPACE)      = 13;

    /* Initialize opcode table pointers */
    init_opcode_pointers (regs);

    regs->configured = 1;

    release_lock (&sysblk.cpulock[cpu]);

#if defined(FEATURE_CONFIGURATION_TOPOLOGY_FACILITY)
    /* Set topology-change-report-pending condition */
    sysblk.topchnge = 1;
#endif /*defined(FEATURE_CONFIGURATION_TOPOLOGY_FACILITY)*/

    return 0;
}


/*-------------------------------------------------------------------*/
/* Uninitialize a CPU                                                */
/*-------------------------------------------------------------------*/
void *cpu_uninit (int cpu, REGS *regs)
{
    if (regs->host)
    {
        obtain_lock (&sysblk.cpulock[cpu]);
        if (regs->guestregs)
        {
            cpu_uninit (cpu, regs->guestregs);
            free (regs->guestregs);
        }
    }

    destroy_condition(&regs->intcond);

    if (regs->host)
    {
#ifdef FEATURE_VECTOR_FACILITY
        /* Mark Vector Facility offline */
        regs->vf->online = 0;
#endif /*FEATURE_VECTOR_FACILITY*/

        /* Remove CPU from all CPU bit masks */
        sysblk.config_mask &= ~CPU_BIT(cpu);
        sysblk.started_mask &= ~CPU_BIT(cpu);
        sysblk.waiting_mask &= ~CPU_BIT(cpu);
        sysblk.regs[cpu] = NULL;
        release_lock (&sysblk.cpulock[cpu]);
    }

#if defined(FEATURE_CONFIGURATION_TOPOLOGY_FACILITY)
    /* Set topology-change-report-pending condition */
    sysblk.topchnge = 1;
#endif /*defined(FEATURE_CONFIGURATION_TOPOLOGY_FACILITY)*/

    return NULL;
}


#endif /*!defined(_GEN_ARCH)*/


/*-------------------------------------------------------------------*/
/* Process interrupt                                                 */
/*-------------------------------------------------------------------*/
void (ATTR_REGPARM(1) ARCH_DEP(process_interrupt))(REGS *regs)
{
    /* Process PER program interrupts */
    if( OPEN_IC_PER(regs) )
        regs->program_interrupt (regs, PGM_PER_EVENT);

    /* Obtain the interrupt lock */
    OBTAIN_INTLOCK(regs);
    OFF_IC_INTERRUPT(regs);
    regs->tracing = (sysblk.inststep || sysblk.insttrace);

    /* Ensure psw.IA is set and invalidate the aia */
    INVALIDATE_AIA(regs);

    /* Perform invalidation */
    if (unlikely(regs->invalidate))
        ARCH_DEP(invalidate_tlbe)(regs, regs->invalidate_main);

    /* Take interrupts if CPU is not stopped */
    if (likely(regs->cpustate == CPUSTATE_STARTED))
    {
        /* Process machine check interrupt */
        if ( OPEN_IC_MCKPENDING(regs) )
        {
            PERFORM_SERIALIZATION (regs);
            PERFORM_CHKPT_SYNC (regs);
            ARCH_DEP (perform_mck_interrupt) (regs);
        }

        /* Process external interrupt */
        if ( OPEN_IC_EXTPENDING(regs) )
        {
            PERFORM_SERIALIZATION (regs);
            PERFORM_CHKPT_SYNC (regs);
            ARCH_DEP (perform_external_interrupt) (regs);
        }

        /* Process I/O interrupt */
        if (IS_IC_IOPENDING)
        {
            if ( OPEN_IC_IOPENDING(regs) )
            {
                PERFORM_SERIALIZATION (regs);
                PERFORM_CHKPT_SYNC (regs);
                ARCH_DEP (perform_io_interrupt) (regs);
            }
            else
                WAKEUP_CPU_MASK(sysblk.waiting_mask);
        }
    } /*CPU_STARTED*/

    /* If CPU is stopping, change status to stopped */
    if (unlikely(regs->cpustate == CPUSTATE_STOPPING))
    {
        /* Change CPU status to stopped */
        regs->opinterv = 0;
        regs->cpustate = CPUSTATE_STOPPED;

        /* Thread exit (note - intlock still held) */
        if (!regs->configured)
            longjmp(regs->exitjmp, SIE_NO_INTERCEPT);

        /* If initial CPU reset pending then perform reset */
        if (regs->sigpireset)
        {
            PERFORM_SERIALIZATION (regs);
            PERFORM_CHKPT_SYNC (regs);
            ARCH_DEP (initial_cpu_reset) (regs);
            RELEASE_INTLOCK(regs);
            longjmp(regs->progjmp, SIE_NO_INTERCEPT);
        }

        /* If a CPU reset is pending then perform the reset */
        if (regs->sigpreset)
        {
            PERFORM_SERIALIZATION (regs);
            PERFORM_CHKPT_SYNC (regs);
            ARCH_DEP(cpu_reset) (regs);
            RELEASE_INTLOCK(regs);
            longjmp(regs->progjmp, SIE_NO_INTERCEPT);
        }

        /* Store status at absolute location 0 if requested */
        if (IS_IC_STORSTAT(regs))
        {
            OFF_IC_STORSTAT(regs);
            ARCH_DEP(store_status) (regs, 0);
            WRMSG (HHC00808, "I", PTYPSTR(regs->cpuad), regs->cpuad);
            /* ISW 20071102 : Do not return via longjmp here. */
            /*    process_interrupt needs to finish putting the */
            /*    CPU in its manual state                     */
            /*
            RELEASE_INTLOCK(regs);
            longjmp(regs->progjmp, SIE_NO_INTERCEPT);
            */
        }
    } /*CPUSTATE_STOPPING*/

    /* Perform restart interrupt if pending */
    if ( IS_IC_RESTART(regs) )
    {
        PERFORM_SERIALIZATION (regs);
        PERFORM_CHKPT_SYNC (regs);
        OFF_IC_RESTART(regs);
        ARCH_DEP(restart_interrupt) (regs);
    } /* end if(restart) */

    /* This is where a stopped CPU will wait */
    if (unlikely(regs->cpustate == CPUSTATE_STOPPED))
    {
        S64 saved_timer = cpu_timer(regs);
        regs->ints_state = IC_INITIAL_STATE;
        sysblk.started_mask ^= regs->cpubit;
        sysblk.intowner = LOCK_OWNER_NONE;

        /* Wait while we are STOPPED */
        wait_condition (&regs->intcond, &sysblk.intlock);

        /* Wait while SYNCHRONIZE_CPUS is in progress */
        while (sysblk.syncing)
            wait_condition (&sysblk.sync_bc_cond, &sysblk.intlock);

        sysblk.intowner = regs->cpuad;
        sysblk.started_mask |= regs->cpubit;
        regs->ints_state |= sysblk.ints_state;
        set_cpu_timer(regs,saved_timer);

        ON_IC_INTERRUPT(regs);

        /* Purge the lookaside buffers */
        ARCH_DEP(purge_tlb) (regs);
#if defined(FEATURE_ACCESS_REGISTERS)
        ARCH_DEP(purge_alb) (regs);
#endif /*defined(FEATURE_ACCESS_REGISTERS)*/

        /* If the architecture mode has changed we must adapt */
        if(sysblk.arch_mode != regs->arch_mode)
            longjmp(regs->archjmp,SIE_NO_INTERCEPT);

        RELEASE_INTLOCK(regs);
        longjmp(regs->progjmp, SIE_NO_INTERCEPT);
    } /*CPUSTATE_STOPPED*/

    /* Test for wait state */
    if (WAITSTATE(&regs->psw))
    {
#ifdef OPTION_MIPS_COUNTING
        regs->waittod = host_tod();
#endif

        /* Test for disabled wait PSW and issue message */
        if( IS_IC_DISABLED_WAIT_PSW(regs) )
        {
            char buf[40];
            WRMSG (HHC00809, "I", PTYPSTR(regs->cpuad), regs->cpuad, str_psw(regs, buf));
            regs->cpustate = CPUSTATE_STOPPING;
            RELEASE_INTLOCK(regs);
            longjmp(regs->progjmp, SIE_NO_INTERCEPT);
        }

        /* Indicate we are giving up intlock */
        sysblk.intowner = LOCK_OWNER_NONE;
        sysblk.waiting_mask |= regs->cpubit;

        /* Wait for interrupt */
        wait_condition (&regs->intcond, &sysblk.intlock);

        /* Wait while SYNCHRONIZE_CPUS is in progress */
        while (sysblk.syncing)
            wait_condition (&sysblk.sync_bc_cond, &sysblk.intlock);

        /* Indicate we now own intlock */
        sysblk.waiting_mask ^= regs->cpubit;
        sysblk.intowner = regs->cpuad;

#ifdef OPTION_MIPS_COUNTING
        /* Calculate the time we waited */
        regs->waittime += host_tod() - regs->waittod;
        regs->waittod = 0;
#endif
        RELEASE_INTLOCK(regs);
        longjmp(regs->progjmp, SIE_NO_INTERCEPT);
    } /* end if(wait) */

    /* Release the interrupt lock */
    RELEASE_INTLOCK(regs);
    return;

} /* process_interrupt */

/*-------------------------------------------------------------------*/
/* Run CPU                                                           */
/*-------------------------------------------------------------------*/
REGS *ARCH_DEP(run_cpu) (int cpu, REGS *oldregs)
{
int     i;
BYTE   *ip;
REGS    regs;
const zz_func *current_opcode_table;
int     aswitch;

#ifdef OPTION_CAPPING
register int    *caplocked = &sysblk.caplocked[cpu];
         LOCK   *caplock = &sysblk.caplock[cpu];
#endif

    if (oldregs)
    {
        memcpy (&regs, oldregs, sizeof(REGS));
        free (oldregs);
        regs.blkloc = swap_byte_U64((U64)((uintptr_t)&regs));
        regs.hostregs = &regs;
        if (regs.guestregs)
            regs.guestregs->hostregs = &regs;
        sysblk.regs[cpu] = &regs;
        release_lock(&sysblk.cpulock[cpu]);
        WRMSG (HHC00811, "I", PTYPSTR(cpu), cpu, get_arch_mode_string(&regs));
    }
    else
    {
        memset(&regs, 0, sizeof(REGS));

        if (cpu_init (cpu, &regs, NULL))
            return NULL;

        WRMSG (HHC00811, "I", PTYPSTR(cpu), cpu, get_arch_mode_string(&regs));

#ifdef FEATURE_VECTOR_FACILITY
        if (regs->vf->online)
            WRMSG (HHC00812, "I", PTYPSTR(cpu), cpu);
#endif /*FEATURE_VECTOR_FACILITY*/
    }

    regs.program_interrupt = &ARCH_DEP(program_interrupt);
#if defined(FEATURE_TRACING)
    regs.trace_br = (func)&ARCH_DEP(trace_br);
#endif

    regs.tracing = (sysblk.inststep || sysblk.insttrace);
    regs.ints_state |= sysblk.ints_state;

    /* Establish longjmp destination for cpu thread exit */
    if (setjmp(regs.exitjmp))
        return cpu_uninit(cpu, &regs);

    /* Establish longjmp destination for architecture switch */
    aswitch = setjmp(regs.archjmp);

    /* Switch architecture mode if appropriate */
    if(sysblk.arch_mode != regs.arch_mode)
    {
        PTT(PTT_CL_INF,"*SETARCH",regs.arch_mode,sysblk.arch_mode,cpu);
        regs.arch_mode = sysblk.arch_mode;
        oldregs = malloc (sizeof(REGS));
        if (oldregs)
        {
            memcpy(oldregs, &regs, sizeof(REGS));
            obtain_lock(&sysblk.cpulock[cpu]);
        }
        else
        {
            char buf[40];
            MSGBUF(buf, "malloc(%d)", (int)sizeof(REGS));
            WRMSG (HHC00813, "E", PTYPSTR(cpu), cpu, buf, strerror(errno));
            cpu_uninit (cpu, &regs);
        }
        return oldregs;
    }

    /* Initialize Architecture Level Set */
    init_als(&regs);
    current_opcode_table=regs.ARCH_DEP(runtime_opcode_xxxx);

    /* Signal cpu has started */
    if(!aswitch)
        signal_condition (&sysblk.cpucond);

    RELEASE_INTLOCK(&regs);

    /* Establish longjmp destination for program check */
    setjmp(regs.progjmp);

    /* Set `execflag' to 0 in case EXecuted instruction did a longjmp() */
    regs.execflag = 0;

    do {
        if (INTERRUPT_PENDING(&regs))
            ARCH_DEP(process_interrupt)(&regs);

#ifdef OPTION_CAPPING
        else if (caplocked[0])
        {
            obtain_lock(caplock);
            release_lock(caplock);
        }
#endif /* #ifdef OPTION_CAPPING */

        ip = INSTRUCTION_FETCH(&regs, 0);

        EXECUTE_INSTRUCTION(current_opcode_table, ip, &regs);
        regs.instcount++;

        /* BHe: I have tried several settings. But 2 unrolled */
        /* executes gives (core i7 at my place) the best results. */
        /* Even a 'do { } while(0);' with several unrolled executes */
        /* and without the 'i' was slower. That surprised me. */
        for(i = 0; i < 128; i++)
        {
            UNROLLED_EXECUTE(current_opcode_table, &regs);
            UNROLLED_EXECUTE(current_opcode_table, &regs);
        }
        regs.instcount += i * 2;

    } while (1);

    /* Never reached */
    return NULL;

} /* end function cpu_thread */

/*-------------------------------------------------------------------*/
/* Process Trace                                                     */
/*-------------------------------------------------------------------*/
void ARCH_DEP(process_trace)(REGS *regs)
{
int     shouldtrace = 0;                /* 1=Trace instruction       */
int     shouldstep = 0;                 /* 1=Wait for start command  */

    /* Test for trace */
    if (CPU_TRACING(regs, 0))
        shouldtrace = 1;

    /* Test for step */
    if (CPU_STEPPING(regs, 0))
        shouldstep = 1;

    /* Display the instruction */
    if (shouldtrace || shouldstep)
    {
        BYTE *ip = regs->ip < regs->aip ? regs->inst : regs->ip;
        ARCH_DEP(display_inst) (regs, ip);
    }

    /* Stop the CPU */
    if (shouldstep)
    {
        REGS *hostregs = regs->hostregs;
        S64 saved_timer[2];

        OBTAIN_INTLOCK(hostregs);
#ifdef OPTION_MIPS_COUNTING
        hostregs->waittod = host_tod();
#endif
        /* The CPU timer is not decremented for a CPU that is in
           the manual state (e.g. stopped in single step mode) */
        saved_timer[0] = cpu_timer(regs);
        saved_timer[1] = cpu_timer(hostregs);
        hostregs->cpustate = CPUSTATE_STOPPED;
        sysblk.started_mask &= ~hostregs->cpubit;
        hostregs->stepwait = 1;
        sysblk.intowner = LOCK_OWNER_NONE;
        while (hostregs->cpustate == CPUSTATE_STOPPED)
        {
            wait_condition (&hostregs->intcond, &sysblk.intlock);
        }
        sysblk.intowner = hostregs->cpuad;
        hostregs->stepwait = 0;
        sysblk.started_mask |= hostregs->cpubit;
        set_cpu_timer(regs,saved_timer[0]);
        set_cpu_timer(hostregs,saved_timer[1]);
#ifdef OPTION_MIPS_COUNTING
        hostregs->waittime += host_tod() - hostregs->waittod;
        hostregs->waittod = 0;
#endif
        RELEASE_INTLOCK(hostregs);
    }
} /* process_trace */


#if !defined(_GEN_ARCH)

#if defined(_ARCHMODE2)
 #define  _GEN_ARCH _ARCHMODE2
 #include "cpu.c"
#endif

#if defined(_ARCHMODE3)
 #undef   _GEN_ARCH
 #define  _GEN_ARCH _ARCHMODE3
 #include "cpu.c"
#endif

/*-------------------------------------------------------------------*/
/* Copy program status word                                          */
/*-------------------------------------------------------------------*/
DLL_EXPORT void copy_psw (REGS *regs, BYTE *addr)
{
REGS cregs;

    memcpy(&cregs, regs, sysblk.regs_copy_len);

    switch(cregs.arch_mode) {
#if defined(_370)
        case ARCH_370:
            s370_store_psw(&cregs, addr);
            break;
#endif
#if defined(_390)
        case ARCH_390:
            s390_store_psw(&cregs, addr);
            break;
#endif
#if defined(_900)
        case ARCH_900:
            z900_store_psw(&cregs, addr);
            break;
#endif
    }
} /* end function copy_psw */

/*-------------------------------------------------------------------*/
/* Display program status word                                       */
/*-------------------------------------------------------------------*/
int display_psw (REGS *regs, char *buf, int buflen)
{
QWORD   qword;                            /* quadword work area      */

    memset(qword, 0, sizeof(qword));

    if( regs->arch_mode != ARCH_900 )
    {
        copy_psw (regs, qword);
        return(snprintf(buf, buflen,
                "%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X",
                qword[0], qword[1], qword[2], qword[3],
                qword[4], qword[5], qword[6], qword[7]));
    }
    else
    {
        copy_psw (regs, qword);
        return(snprintf(buf, buflen,
                "%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X "
                "%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X",
                qword[0], qword[1], qword[2], qword[3],
                qword[4], qword[5], qword[6], qword[7],
                qword[8], qword[9], qword[10], qword[11],
                qword[12], qword[13], qword[14], qword[15]));
    }

} /* end function display_psw */

/*-------------------------------------------------------------------*/
/* Print program status word into buffer                             */
/*-------------------------------------------------------------------*/
char *str_psw (REGS *regs, char *buf)
{
QWORD   qword;                            /* quadword work area      */

    memset(qword, 0, sizeof(qword));

    if( regs->arch_mode != ARCH_900 )
    {
        copy_psw (regs, qword);
        sprintf(buf, "%2.2X%2.2X%2.2X%2.2X %2.2X%2.2X%2.2X%2.2X",
                qword[0], qword[1], qword[2], qword[3],
                qword[4], qword[5], qword[6], qword[7]);
    }
    else
    {
        copy_psw (regs, qword);
        sprintf(buf, "%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X "
                     "%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X",
                qword[0], qword[1], qword[2], qword[3],
                qword[4], qword[5], qword[6], qword[7],
                qword[8], qword[9], qword[10], qword[11],
                qword[12], qword[13], qword[14], qword[15]);
    }
    return(buf);

} /* end function str_psw */


const char* arch_name[GEN_MAXARCH] =
{
#if defined(_370)
        _ARCH_370_NAME,
#endif
#if defined(_390)
        _ARCH_390_NAME,
#endif
#if defined(_900)
        _ARCH_900_NAME
#endif
};

const char* get_arch_mode_string(REGS* regs)
{
#if defined(FEATURE_S380)
    if ( sysblk.s380 ) return ( "S/380" );
#endif
    if (!regs) return arch_name[sysblk.arch_mode];
    else return arch_name[regs->arch_mode];
}

#endif /*!defined(_GEN_ARCH)*/
