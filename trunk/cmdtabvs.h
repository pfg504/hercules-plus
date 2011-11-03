/* CMDTABVS.H   (c) Copyright Silence Dogood 2011                    */
/*              Route VM Style SET Hercules panel commands           */
/*              to the appropriate functions                         */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#define _CMDTABVS_H_
#define help_cmd_desc          "Display SET subcommand help"
#define help_cmd_help          "Display help for SET subcommands"

#if defined(_FW_REF)
  #if defined(_FW_REF_CMD)
    #undef _FW_REF_CMD
  #endif
  #if defined(_FW_REF_ABR)
    #undef _FW_REF_ABR
  #endif
  #define _FW_REF_CMD(_s,   _f,_t,_d,_l)  static int (_f)( CMDFUNC_ARGS_PROTO );
  #define _FW_REF_ABR(_s,_a,_f,_t,_d,_l)  static int (_f)( CMDFUNC_ARGS_PROTO );
#endif

#if 0               // Comments from cmdinc.h
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
#endif

// Keep these sorted ...
CMDABBR( "help",   4,              help_cmd,              SYSCMD,             help_cmd_desc,         help_cmd_help     )
