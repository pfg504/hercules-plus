/* HSCDEF.C     (c) Copyright Silence Dogood, 2011                   */
/*              Execute Hercules System Commands - DEFINE            */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/* This module implements the various Hercules System Console        */
/* DEFINE commands.                                                  */
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

#define _HSCDEF_C_
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

int define_vm_cmd(int argc, char *argv[],char *cmdline)
{
    UNREFERENCED(argc);
    UNREFERENCED(argv);
    UNREFERENCED(cmdline);
    return 0;
}
/* HSCDEF.C End-of-text */

