/* FEAT390.H    (c) Copyright Jan Jaeger, 2000-2011                  */
/*              ESA/390 feature definitions                          */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*-------------------------------------------------------------------*/
/* This file defines the architectural features which are included   */
/* at compilation time for ESA/390 mode                              */
/*-------------------------------------------------------------------*/
 
/* This file MUST NOT contain #undef statements */
#if defined(OPTION_390_MODE)
#define _ARCH_390_NAME "ESA/390"

#define FEATURE_4K_STORAGE_KEYS
#define FEATURE_ACCESS_REGISTERS
#define FEATURE_ADDRESS_LIMIT_CHECKING
#define FEATURE_BASIC_FP_EXTENSIONS
#define FEATURE_BIMODAL_ADDRESSING
#define FEATURE_BINARY_FLOATING_POINT
#define FEATURE_BRANCH_AND_SET_AUTHORITY
#define FEATURE_BROADCASTED_PURGING
#define FEATURE_CANCEL_IO_FACILITY /* comment if FEATURE_S370_CHANNEL used */
#define FEATURE_CALLED_SPACE_IDENTIFICATION
#define FEATURE_CHANNEL_SUBSYSTEM  /* comment if FEATURE_S370_CHANNEL used */
//#define FEATURE_CHANNEL_SWITCHING  /* comment if FEATURE_CHANNEL_SUBSYSTEM used */
#define FEATURE_CHECKSUM_INSTRUCTION
#define FEATURE_CHSC
#define FEATURE_COMPARE_AND_MOVE_EXTENDED
#define FEATURE_COMPRESSION
#define FEATURE_CPU_RECONFIG
#define FEATURE_DUAL_ADDRESS_SPACE
#define FEATURE_EMULATE_VM
#define FEATURE_ETF2_ENHANCEMENT                                /*@ZA*/
#define FEATURE_ETF3_ENHANCEMENT                                /*@ZA*/
#define FEATURE_EXPANDED_STORAGE
#define FEATURE_EXPEDITED_SIE_SUBSET
#define FEATURE_EXTENDED_STORAGE_KEYS
#define FEATURE_EXTENDED_TOD_CLOCK
#define FEATURE_EXTENDED_TRANSLATION
#define FEATURE_EXTENDED_TRANSLATION_FACILITY_2
#define FEATURE_EXTENDED_TRANSLATION_FACILITY_3                 /*@ZA*/
#define FEATURE_EXTERNAL_INTERRUPT_ASSIST
#define FEATURE_FAST_SYNC_DATA_MOVER
#define FEATURE_FETCH_PROTECTION_OVERRIDE
#define FEATURE_FPS_ENHANCEMENT                                 /*DFP*/
#define FEATURE_FPS_EXTENSIONS
#define FEATURE_HERCULES_DIAGCALLS
#define FEATURE_HEXADECIMAL_FLOATING_POINT
#define FEATURE_HFP_EXTENSIONS
#define FEATURE_HFP_MULTIPLY_ADD_SUBTRACT
#define FEATURE_HOST_RESOURCE_ACCESS_FACILITY
#define FEATURE_HYPERVISOR
#define FEATURE_IMMEDIATE_AND_RELATIVE
#define FEATURE_INCORRECT_LENGTH_INDICATION_SUPPRESSION
#define FEATURE_INTEGRATED_3270_CONSOLE
//#define FEATURE_INTEGRATED_ASCII_CONSOLE
#define FEATURE_INTERPRETIVE_EXECUTION
#define FEATURE_IO_ASSIST
#define FEATURE_LOCK_PAGE
#define FEATURE_LINKAGE_STACK
#define FEATURE_MESSAGE_SECURITY_ASSIST
#define FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_1
#define FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_2
#define FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_3
#define FEATURE_MESSAGE_SECURITY_ASSIST_EXTENSION_4
#define FEATURE_MOVE_PAGE_FACILITY_2
#define FEATURE_MPF_INFO
#define FEATURE_MSSF_CALL
#define FEATURE_MULTIPLE_CONTROLLED_DATA_SPACE
#define FEATURE_MVS_ASSIST
#define FEATURE_PAGE_PROTECTION
#define FEATURE_PERFORM_LOCKED_OPERATION
#define FEATURE_PER
#define FEATURE_PER2
#define FEATURE_PRIVATE_SPACE
#define FEATURE_PROTECTION_INTERCEPTION_CONTROL
#define FEATURE_QUEUED_DIRECT_IO
#define FEATURE_REGION_RELOCATE
#define FEATURE_RESUME_PROGRAM
//#define FEATURE_S370_CHANNEL  /* comment if FEATURE_CHANNEL_SUBSYSTEM used */
#define FEATURE_S390_DAT
#define FEATURE_SCEDIO
#define FEATURE_SERVICE_PROCESSOR
#define FEATURE_SET_ADDRESS_SPACE_CONTROL_FAST
#define FEATURE_SQUARE_ROOT
#define FEATURE_STORAGE_KEY_ASSIST
#define FEATURE_STORAGE_PROTECTION_OVERRIDE
#define FEATURE_STORE_SYSTEM_INFORMATION
#define FEATURE_STRING_INSTRUCTION
#define FEATURE_SUBSPACE_GROUP
#define FEATURE_SUPPRESSION_ON_PROTECTION
#define FEATURE_SYSTEM_CONSOLE
#define FEATURE_TEST_BLOCK
#define FEATURE_TRACING
#define FEATURE_WAITSTATE_ASSIST
#define FEATURE_STORE_FACILITY_LIST
#define FEATURE_STORE_FACILITY_LIST_EXTENDED
#define FEATURE_VM_BLOCKIO
// #define FEATURE_VECTOR_FACILITY

#endif /*defined(OPTION_390_MODE)*/
/* end of FEAT390.H */