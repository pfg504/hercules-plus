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

CMDABBR( "help",   4,              qhelp_cmd,              SYSCMD,             help_cmd_desc,         help_cmd_help     )
CMDABBR( "cpuid",  5,              qcpuid_cmd,             SYSCMD,             cpuid_cmd_desc,        cpuid_cmd_help     )
CMDABBR( "pid",    3,              qpid_cmd,               SYSCMD,             pid_cmd_desc,          NULL                )
CMDABBR( "ports",  5,              qports_cmd,             SYSCMD,             ports_cmd_desc,        NULL                )
CMDABBR( "processor",4,            qproc_cmd,              SYSCMD,             proc_cmd_desc,         NULL                )
CMDABBR( "storage", 4,             qstor_cmd,              SYSCMD,             stor_cmd_desc,         NULL                )
#if defined( OPTION_CONFIG_SYMBOLS )
CMDABBR( "pfkeys",  2,             qpfkeys_cmd,            SYSCMD,             pfkeys_cmd_desc,       NULL                )
#endif