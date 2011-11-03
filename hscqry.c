/* HSCQRY.C     (c) Copyright Silence Dogood, 2011                   */
/*              Execute Hercules System Commands - QUERY             */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/* This module implements the various Hercules System Console        */
/* QUERY commands.                                                   */
/*-------------------------------------------------------------------*/

/*
   Standard conventions are:

     argc             contains the number of elements in argv[]
     argv[0]          contains the actual command
     argv[1..argc-1]  contains the optional arguments
     cmdline          contains the original command line

     returncode:

     0 = Success

     < 0 Error: Command not executed

     > 1 Failure:  one or more functions could not complete

   int test_cmd(int argc, char *argv[], char *cmdline)
   {

   .
   .
   .
   return rc

   }

*/

#include "hstdinc.h"

#define _HSCQRY_C_
#define _HENGINE_DLL_

#include "hercules.h"
#include "devtype.h"
#include "opcode.h"
#include "history.h"
#include "httpmisc.h"

#if defined(OPTION_FISHIO)
#include "w32chan.h"
#endif /* defined(OPTION_FISHIO) */

#include "tapedev.h"
#include "dasdtab.h"
#include "ctcadpt.h"

#include "cmdinc.h"

int query_vm_cmd(int argi, char *argx[],char *cmdline)
{
    char           *argn[MAX_ARGS+1];   // Our copy of pointers
    char          **argv = argn;        // Our copy of the ptr to pointers
    int             argc;               // Our copy
    CMDTAB*         pCmdTab;
    size_t          cmdlen, matchlen;
    int             rc;                 // Default to invalid command

    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);

    VERIFY( argi <= MAX_ARGS );                     // This should always be true
    argc = ( argi <= MAX_ARGS ? argi : MAX_ARGS );
    for( rc = 0; rc < argc; rc++ )
        argn[rc] = argx[rc];

    argc--;         // Decrement Count to skip past "QUERY"
    argv++;         // Move pointer forward past "QUERY"
    
    rc = HERRINVCMD;
    
    for (pCmdTab = cmdtab; pCmdTab->statement; pCmdTab++)
    {
        if (1
            && pCmdTab->function
            && (pCmdTab->type & sysblk.sysgroup)
            /* Commands issues through DIAG8 must NOT be part of the SYSNDIAG8 group */
            && (!(sysblk.diag8cmd & DIAG8CMD_RUNNING) || !(pCmdTab->type & SYSNDIAG8))
        )
        {
            cmdlen = pCmdTab->mincmdlen ? pCmdTab->mincmdlen : strlen( pCmdTab->statement );
            matchlen = MAX( strlen(argv[0]), cmdlen );

            if (!strncasecmp( argv[0], pCmdTab->statement, matchlen ))
            {
                char cmd[256]; /* (copy of command name) */

                /* Make full-length copy of the true command's name */
                strlcpy( cmd, pCmdTab->statement, sizeof(cmd) );
                argv[0] = cmd; /* (since theirs may be abbreviated) */

                 /* Process the Hercules command */
                rc = pCmdTab->function( CMDFUNC_ARGS );
                break;
            }
            /* end strncasecmp( table entry match ) */
        }
        /* end if ( valid table entry ) */
    }
    /* end for ( search table ) */
    return 0;
}
/*-------------------------------------------------------------------*/
/* help command                                                      */
/*-------------------------------------------------------------------*/
static int help_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argv);
    UNREFERENCED(argc);
    return 0;
}

/*-------------------------------------------------------------------*/
/* cpuid command                                                     */
/*-------------------------------------------------------------------*/
static int cpuid_cmd(int argc, char *argv[], char *cmdline)
{
    /* Note: The machine-type must be set before the message is      */
    /*       issued due to gcc incorrectly handling substitution     */
    /*       of the third and fourth variables on some platforms.    */

    char *model = str_modelcapa();
    char *manuf = str_manufacturer();
    char *plant = str_plant();
    U16   machinetype = ( sysblk.cpuid >> 16 ) & 0xFFFF;
    U32   sequence    = ( sysblk.cpuid >> 32 ) & 0x00FFFFFF;

    UNREFERENCED(cmdline);
    UNREFERENCED(argv);

    if (argc != 1)
    {
        WRMSG( HHC02299, "E", argv[0] );
        return -1;
    }

    WRMSG( HHC17004, "I",  sysblk.cpuid );
    WRMSG( HHC17005, "I",  machinetype,
                           model,
                           manuf,
                           plant,
                           sequence );
    return 0;
}

#if       defined( OPTION_CONFIG_SYMBOLS )
/*-------------------------------------------------------------------*/
/* pfkeys command                                                    */
/*-------------------------------------------------------------------*/
static int pfkeys_cmd(int argc, char *argv[], char *cmdline)
{
    int     i;
    char    szPF[6];
    char   *pszVAL;

    UNREFERENCED(cmdline);
    UNREFERENCED(argv);

    if (argc != 1)
    {
        WRMSG( HHC17000, "E" );
        return -1;
    }
#if defined ( _MSVC_ )
    for ( i = 1; i <= 48; i++ )
#else
    for ( i = 1; i <= 20; i++ )
#endif
    {
        MSGBUF( szPF, "PF%02d", i );
        szPF[4] = '\0';

        pszVAL = (char*)get_symbol(szPF);

        if ( pszVAL == NULL )
            pszVAL = "UNDEFINED";

        WRMSG( HHC17199, "I", szPF, pszVAL );
    }

    return 0;
}
#endif // defined( OPTION_CONFIG_SYMBOLS )

/*-------------------------------------------------------------------*/
/* pid command                                                       */
/*-------------------------------------------------------------------*/
static int pid_cmd(int argc, char *argv[], char *cmdline)
{
    UNREFERENCED(cmdline);
    UNREFERENCED(argv);
    UNREFERENCED(argc);

    if (argc != 1)
    {
        WRMSG( HHC17000, "E" );
        return -1;
    }

    WRMSG( HHC17013, "I", sysblk.hercules_pid );

    return 0;
}

/*-------------------------------------------------------------------*/
/* ports command                                                     */
/*-------------------------------------------------------------------*/
static int ports_cmd(int argc, char *argv[], char *cmdline)
{
    char buf[64];

    UNREFERENCED(cmdline);
    UNREFERENCED(argv);

    if (argc != 1)
    {
        WRMSG( HHC17000, "E" );
        return -1;
    }

#if defined(OPTION_HTTP_SERVER)
    MSGBUF( buf, "on port %s with %s", http_get_port(), http_get_portauth());
    WRMSG(HHC17001, "I", "HTTP", buf);
#endif /*defined(OPTION_HTTP_SERVER)*/

    if ( sysblk.shrdport > 0 )
    {
        MSGBUF( buf, "on port %u", sysblk.shrdport);
        WRMSG( HHC17001, "I", "Shared DASD", buf);
    }
    else
    {
        WRMSG( HHC17002, "I", "Shared DASD");
    }

    if (strchr(sysblk.cnslport, ':') == NULL)
    {
        MSGBUF( buf, "on port %s", sysblk.cnslport);
    }
    else
    {
        char *serv;
        char *host = NULL;
        char *port = strdup(sysblk.cnslport);

        if ((serv = strchr(port,':')))
        {
            *serv++ = '\0';
            if (*port)
                host = port;
        }
        MSGBUF( buf, "for host %s on port %s", host, serv);
        free( port );
    }
    WRMSG( HHC17001, "I", "Console", buf);

    return 0;
}

/*-------------------------------------------------------------------*/
/* proc command                                                      */
/*-------------------------------------------------------------------*/
static int proc_cmd(int argc, char *argv[], char *cmdline)
{
    int i, j, k;
    int cpupct = 0;
    U32 mipsrate = 0;
#if defined(_MSVC_)
    char msgbuf[128];
#endif

    UNREFERENCED(cmdline);
    UNREFERENCED(argv);

    if (argc != 1)
    {
        WRMSG( HHC17000, "E" );
        return -1;
    }

/* VS does not allow for macros to be split with ifdefs */

    {
#ifdef    _FEATURE_VECTOR_FACILITY
        u_int nv = sysblk.numvec;
#else  /*!_FEATURE_VECTOR_FACILITY*/
        u_int nv = 0;
#endif /* _FEATURE_VECTOR_FACILITY*/
        WRMSG( HHC17007, "I",   sysblk.cpus, nv, sysblk.maxcpu - sysblk.cpus, sysblk.maxcpu );
    }

    for ( i = j = 0; i < sysblk.maxcpu; i++ )
    {
        if ( IS_CPU_ONLINE(i) && sysblk.regs[i]->cpustate == CPUSTATE_STARTED )
        {
            j++;
            cpupct += sysblk.regs[i]->cpupct;
        }
    }

#ifdef OPTION_MIPS_COUNTING
    mipsrate = sysblk.mipsrate;

    WRMSG( HHC17008, "I", j, ( j == 0 ? 0 : ( cpupct / j ) ),
                    mipsrate / 1000000, ( mipsrate % 1000000 ) / 10000,
                    sysblk.siosrate, "" );
#endif

#if defined(OPTION_CAPPING)

    if ( sysblk.capvalue > 0 )
    {
        cpupct = 0;
        mipsrate = 0;
        for ( i = k = 0; i < sysblk.maxcpu; i++ )
        {
            if ( IS_CPU_ONLINE(i) &&
                ( sysblk.ptyp[i] == SCCB_PTYP_CP || sysblk.ptyp[i] == SCCB_PTYP_IFA ) &&
                 sysblk.regs[i]->cpustate == CPUSTATE_STARTED )
            {
                k++;
                cpupct += sysblk.regs[i]->cpupct;
                mipsrate += sysblk.regs[i]->mipsrate;
            }
        }

        if ( k > 0 && k != j )
        {
            WRMSG( HHC17011, "I", k, ( k == 0 ? 0 : ( cpupct / k ) ),
                                  mipsrate / 1000000,
                                ( mipsrate % 1000000 ) / 10000 );
        }
    }
#endif
    for ( i = 0; i < sysblk.maxcpu; i++ )
    {
        if ( IS_CPU_ONLINE(i) )
        {
            char *pmsg = "";
#if defined(_MSVC_)
            FILETIME ftCreate, ftExit, ftKernel, ftUser;

            if ( GetThreadTimes(win_thread_handle(sysblk.cputid[i]), &ftCreate, &ftExit, &ftKernel, &ftUser) != 0 )
            {
                char    msgKernel[64];
                char    msgUser[64];
                char    yy[8], mm[8], dd[8], hh[8], mn[8], ss[8], ms[8];

                SYSTEMTIME  st;

                FileTimeToSystemTime( &ftKernel, &st );
                st.wYear    -= 1601;
                st.wDay     -= 1;
                st.wMonth   -= 1;

                MSGBUF( yy, "%02d", st.wYear );
                MSGBUF( mm, "%02d", st.wMonth );
                MSGBUF( dd, "%02d", st.wDay );
                MSGBUF( hh, "%02d", st.wHour );
                MSGBUF( mn, "%02d", st.wMinute );
                MSGBUF( ss, "%02d", st.wSecond );
                MSGBUF( ms, "%03d", st.wMilliseconds );

                if ( st.wYear != 0 )
                    MSGBUF( msgKernel, "%s/%s/%s %s:%s:%s.%s", yy, mm, dd, hh, mn, ss, ms );
                else if ( st.wMonth != 0 )
                    MSGBUF( msgKernel, "%s/%s %s:%s:%s.%s", mm, dd, hh, mn, ss, ms );
                else if ( st.wDay != 0 )
                    MSGBUF( msgKernel, "%s %s:%s:%s.%s", dd, hh, mn, ss, ms );
                else
                    MSGBUF( msgKernel, "%s:%s:%s.%s", hh, mn, ss, ms );

                FileTimeToSystemTime( &ftUser, &st );
                st.wYear    -= 1601;
                st.wDay     -= 1;
                st.wMonth   -= 1;

                MSGBUF( yy, "%02d", st.wYear );
                MSGBUF( mm, "%02d", st.wMonth );
                MSGBUF( dd, "%02d", st.wDay );
                MSGBUF( hh, "%02d", st.wHour );
                MSGBUF( mn, "%02d", st.wMinute );
                MSGBUF( ss, "%02d", st.wSecond );
                MSGBUF( ms, "%03d", st.wMilliseconds );

                if ( st.wYear != 0 )
                    MSGBUF( msgUser, "%s/%s/%s %s:%s:%s.%s", yy, mm, dd, hh, mn, ss, ms );
                else if ( st.wMonth != 0 )
                    MSGBUF( msgUser, "%s/%s %s:%s:%s.%s", mm, dd, hh, mn, ss, ms );
                else if ( st.wDay != 0 )
                    MSGBUF( msgUser, "%s %s:%s:%s.%s", dd, hh, mn, ss, ms );
                else
                    MSGBUF( msgUser, "%s:%s:%s.%s", hh, mn, ss, ms );

                MSGBUF( msgbuf, " - Host Kernel(%s) User(%s)", msgKernel, msgUser );

                pmsg = msgbuf;
            }
#endif
            mipsrate = sysblk.regs[i]->mipsrate;
            WRMSG( HHC17009, "I", PTYPSTR(i), i,
                                ( sysblk.regs[i]->cpustate == CPUSTATE_STARTED ) ? '-' :
                                ( sysblk.regs[i]->cpustate == CPUSTATE_STOPPING ) ? ':' : '*',
                                  sysblk.regs[i]->cpupct,
                                  mipsrate / 1000000,
                                ( mipsrate % 1000000 ) / 10000,
                                  sysblk.regs[i]->siosrate,
                                  pmsg );
        }
    }

    WRMSG( HHC17010, "I" );

    return 0;
}


/*-------------------------------------------------------------------*/
/* fmt_memsize routine for qstor                                     */
/*-------------------------------------------------------------------*/
static char *fmt_memsize( const U64 memsize )
{
    // Mainframe memory and DASD amounts are reported in 2**(10*n)
    // values, (x_iB international format, and shown as x_ or x_B, when
    // x >= 1024; x when x < 1024). Open Systems and Windows report
    // memory in the same format, but report DASD storage in 10**(3*n)
    // values. (Thank you, various marketing groups and international
    // standards committees...)

    // For Hercules, mainframe oriented reporting characteristics will
    // be formatted and shown as x_, when x >= 1024, and as x when x <
    // 1024. Reporting of Open Systems and Windows specifics should
    // follow the international format, shown as x_iB, when x >= 1024,
    // and x or xB when x < 1024. Reporting is done at the highest
    // integral boundary.

    // Format storage in 2**(10*n) values at the highest integral
    // integer boundary.

    const  char suffix[9] = {0x00, 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'};
    static char fmt_mem[128];    // Max of 21 bytes used for U64
    u_int   i = 0;
    U64 mem = memsize;

    if (mem)
        for (i = 0; i < sizeof(suffix); i++)
            {
                if (mem & 0x3FF)
                    break;
                mem >>= 10;
            }

    MSGBUF( fmt_mem, "%"I64_FMT"u %c", mem, suffix[i]);

    return fmt_mem;
}


/*-------------------------------------------------------------------*/
/* Query STORage command                                             */
/*-------------------------------------------------------------------*/
static int stor_cmd(int argc, char *argv[], char *cmdline)
{
    BYTE    display_main = FALSE;
    BYTE    display_xpnd = FALSE;

    UNREFERENCED(cmdline);

    if ( argc < 2 )
        display_main = display_xpnd = TRUE;
    else
    {
        u_int i;
        for (i = 1; i < (u_int)argc; i++)
        {
            char check[16];
            strnupper(check, argv[i], (u_int)sizeof(check));    // Uppercase for multiple checks
            if ( strabbrev("MAINSIZE", check, 1) )
                display_main = TRUE;
            else if ( strabbrev("XPNDSIZE", check, 1) ||
                      strabbrev("EXPANDED", check, 1) )
                display_xpnd = TRUE;
            else
            {
                WRMSG( HHC02205, "E", argv[1], "; either 'mainsize' or 'xpndsize' is valid" );
                return -1;
            }
        }
    }

    if ( display_main )
    {
        U64 mainsize = sysblk.mainsize;
        if (!sysblk.maxcpu && mainsize <= _64_KILOBYTE )
            mainsize = 0;
        WRMSG( HHC17003, "I", "MAIN", fmt_memsize((U64)mainsize),
                              "main", sysblk.mainstor_locked ? "":"not " );
    }
    if ( display_xpnd )
    {
        WRMSG( HHC17003, "I", "EXPANDED", fmt_memsize((U64)sysblk.xpndsize << 12),
                              "xpnd", sysblk.xpndstor_locked ? "":"not "  );
    }
    return 0;
}

/* HSCQRY.C End-of-text */
#undef _HSCQRY_C_
