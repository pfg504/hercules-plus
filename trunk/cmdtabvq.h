/* CMDTABVQ.H   (c) Copyright Silence Dogood 2011                    */
/*              Route VM Style QUERY Hercules panel commands         */
/*              to the appropriate functions                         */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#define _CMDTABVQ_H_

#define help_cmd_desc          "Display Query subcommand Help"
#define help_cmd_help          "Display help for Query subcommands"
#define cpuid_cmd_desc         "Display cpuid"
#define cpuid_cmd_help         "Display cpuid and STSI results presented to the SCP\n"

#define pfkeys_cmd_desc        "Display the current PF Key settings"
#define pid_cmd_desc           "Display Process ID of Hercules"
#define ports_cmd_desc         "Display TCP/IP ports in use"
#define proc_cmd_desc          "Display processors type and utilization"
#define stor_cmd_desc          "Display main and expanded storage values"

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

CMDABBR( "help",   4,              help_cmd,              SYSCMD,             help_cmd_desc,         help_cmd_help     )
CMDABBR( "cpuid",  5,              cpuid_cmd,             SYSCMD,             cpuid_cmd_desc,        cpuid_cmd_help     )
CMDABBR( "pid",    3,              pid_cmd,               SYSCMD,             pid_cmd_desc,          NULL                )
CMDABBR( "ports",  2,              ports_cmd,             SYSCMD,             ports_cmd_desc,        NULL                )
CMDABBR( "processor",4,            proc_cmd,              SYSCMD,             proc_cmd_desc,         NULL                )
CMDABBR( "storage", 3,             stor_cmd,              SYSCMD,             stor_cmd_desc,         NULL                )
#if defined( OPTION_CONFIG_SYMBOLS )
CMDABBR( "pfkeys",  2,             pfkeys_cmd,            SYSCMD,             pfkeys_cmd_desc,       NULL                )
#endif