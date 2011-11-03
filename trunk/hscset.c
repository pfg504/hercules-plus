/* HSCSET.C     (c) Copyright Silence Dogood, 2011                   */
/*              Execute Hercules System Commands - SET               */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/* This module implements the various Hercules System Console        */
/* SET commands.                                                     */
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

#define _HSCSET_C_
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

int set_vm_cmd(int argi, char *argx[],char *cmdline)
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

/* HSCSET.C End-of-text */
#undef _HSCSET_C_

