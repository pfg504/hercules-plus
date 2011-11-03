/* CMDINC.H     (c) Copyright Silence Dogood 2011                    */
/*              Route all Hercules configuration statements          */
/*              and panel commands to the appropriate functions      */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#if !defined(_CMDINC_H_)
#define _CMDINC_H_


/*-------------------------------------------------------------------*/
/* Prototype of a command processing function                        */
/*-------------------------------------------------------------------*/
#define     CMDFUNC_ARGS_PROTO  int argc, char *argv[], char *cmdline
#define     CMDFUNC_ARGS            argc,       argv,         cmdline
typedef int CMDFUNC( CMDFUNC_ARGS_PROTO );

/*-------------------------------------------------------------------*/
/* Handle externally defined commands. If a command's primary        */
/* processing function is defined in some other module other than    */
/* this one, the below EXTCMD macro will route our module to it.     */
/* This macro must be #defined before "cmdtab.h" is #included.       */
/*-------------------------------------------------------------------*/

#define EXTCMD(      _cmd )    call_ ## _cmd
#define CALL_EXTCMD( _cmd )                        \
                                                   \
  int call_ ## _cmd ( CMDFUNC_ARGS_PROTO )  {      \
      return   _cmd ( CMDFUNC_ARGS       ); }

/*-------------------------------------------------------------------*/
/* Layout of CMDTAB Command Routing Table                            */
/*-------------------------------------------------------------------*/
struct CMDTAB
{
    const char    *statement;           /* Command or statement      */
    const char    *shortdesc;           /* Short description         */
    const char    *longdesc;            /* Long description          */
    CMDFUNC       *function;            /* Handler function          */
    const size_t   mincmdlen;           /* Minimum abbreviation      */
    BYTE           type;                /* Command type (see below)  */
};
typedef struct CMDTAB CMDTAB;

/*-------------------------------------------------------------------*/
/* Command types                                                     */
/*-------------------------------------------------------------------*/
#define DISABLED       0x00             /* Disabled command          */
#define SYSOPER        0x01             /* System Operator           */
#define SYSMAINT       0x02             /* System Maintainer         */
#define SYSPROG        0x04             /* Systems Programmer        */
#define SYSNONE        0x08             /* Valid with no group       */
#define SYSCONFIG      0x10             /* System Configuration      */
#define SYSDEVEL       0x20             /* System Developer          */
#define SYSDEBUG       0x40             /* Enable Debugging          */
#define SYSNDIAG8      0x80             /* Invalid for DIAG008       */
#define SYSALL         0x7F             /* Enable for any lvls       */

/* Combinations as a one-word mnemonic */

#define SYSCMD              (SYSOPER | SYSMAINT | SYSPROG | SYSCONFIG | SYSDEVEL | SYSDEBUG)
#define SYSCMDNDIAG8        (SYSCMD    | SYSNDIAG8)
#define SYSCFGNDIAG8        (SYSCONFIG | SYSNDIAG8)
#define SYSALLNDIAG8        (SYSALL    | SYSNDIAG8)
#define SYSCMDNOPER         (SYSCMD       - SYSOPER)
#define SYSCMDNOPERNDIAG8   (SYSCMDNDIAG8 - SYSOPER)
#define SYSCMDNOPERNPROG    (SYSCMDNOPER  - SYSPROG)
#define SYSPROGDEVEL        (SYSPROG | SYSDEVEL)

/*-------------------------------------------------------------------*/
/* Macros used to define entries in the command table                */
/*-------------------------------------------------------------------*/
#define _COMMAND(_s,   _f,_t,_d,_l)  {(_s),(_d),(_l),(_f),(0), (_t)},
#define _CMDABBR(_s,_a,_f,_t,_d,_l)  {(_s),(_d),(_l),(_f),(_a),(_t)},

/*-------------------------------------------------------------------*/
/* Macros used to declare the functions before they're referenced.   */
/*-------------------------------------------------------------------*/
#define _FW_REF_CMD(_s,   _f,_t,_d,_l)  extern int (_f)( CMDFUNC_ARGS_PROTO );
#define _FW_REF_ABR(_s,_a,_f,_t,_d,_l)  extern int (_f)( CMDFUNC_ARGS_PROTO );

/*-------------------------------------------------------------------*/
/* Create forward references for all commands in the command table   */
/*-------------------------------------------------------------------*/
#define _FW_REF
#define COMMAND             _FW_REF_CMD
#define CMDABBR             _FW_REF_ABR
#if defined(_CMDTAB_C_)
#include "cmdtab.h"         /* (declare all command functions) */
#elif defined(_HSCQRY_C_)
#include "cmdtabvq.h"
#elif defined(_HSCSET_C_)
#include "cmdtabvs.h"
#elif defined(_HSCDEF_C_)
#include "cmdtabvd.h"
#elif defined(_HSCIND_C_)
#include "cmdtabvi.h"
#endif

#undef  COMMAND             /* (will be re-#defined below) */
#undef  CMDABBR             /* (will be re-#defined below) */
#undef _FW_REF

/*-------------------------------------------------------------------*/
/*              THE ACTUAL COMMAND TABLE ITSELF                      */
/*-------------------------------------------------------------------*/
#define COMMAND             _COMMAND
#define CMDABBR             _CMDABBR
static  CMDTAB   cmdtab[]  = {              /* (COMMAND table)       */
#if defined(_CMDTAB_C_)
#include "cmdtab.h"         /* (declare all command functions) */
#elif defined(_HSCQRY_C_)
#include "cmdtabvq.h"
#elif defined(_HSCSET_C_)
#include "cmdtabvs.h"
#elif defined(_HSCDEF_C_)
#include "cmdtabvd.h"
#elif defined(_HSCIND_C_)
#include "cmdtabvi.h"
#endif
    COMMAND ( NULL, NULL, 0, NULL, NULL )     /* (end of table)        */
};
#endif // defined(_CMDINC_H_)
