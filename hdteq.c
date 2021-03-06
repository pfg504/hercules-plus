/* HDTEQ.C      (c) Copyright Jan Jaeger, 2003-2011                  */
/*              (c)Copyright Silence Dogood 2011                     */
/*              Hercules Dynamic Loader                              */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

#include "hstdinc.h"

#include "hercules.h"


typedef struct _DTEQ {
    char *alias;
    char *name;
} DTEQ;


static DTEQ dteq[] = {
/*
    This table provides aliases for device types, such that various 
    device types may be mapped to a common loadable module.

    The only purpose of this table is to associate the right loadable 
    module with a specific device type, before the device type in 
    question has been registered.  This table will not be searched 
    for registered device types or if the specific loadable module exists.

       device type requested 
       |
       |         base device support
       |         |
       V         V                                                   */

//  { "3390",   "3990"  },
//  { "3380",   "3990"  },

    { "1052",   "3270"  },      // console I&O
    { "1053",   "3270"  },      // console O
    { "3215",   "3270"  },
    { "3138",   "3270"  },
    { "3148",   "3270"  },
    { "3158",   "3270"  },
    { "3277",   "3270"  },
    { "3278",   "3270"  },
    { "3279",   "3270"  },
    { "3284",   "3270"  },
    { "3286",   "3270"  },
    { "3287",   "3270"  },
    { "3288",   "3270"  },
    { "3289",   "3270"  },
    { "4250",   "3270"  },
    { "SYSG",   "3270"  },

    { "1052-C", "1052c" },
    { "1053-C", "1052c" },      // output only
    { "3215-C", "1052c" },

    { "1442R",  "3505"  },
    { "2501",   "3505"  },
    { "2520R",  "3505"  },
    { "2540R",  "3505"  },
    { "3504",   "3505"  },

    { "1442P",  "3525"  },
    { "2520P",  "3525"  },
    { "2540P",  "3525"  },

    { "1443",   "1403"  },
    { "3203",   "1403"  },
    { "3211",   "1403"  },
    { "3262",   "1403"  },
    { "3289E",  "1403"  },
    { "4245",   "1403"  },
    { "4248",   "1403"  },

    { "3410",   "3420"  },
    { "3411",   "3420"  },
//  { "3420",   "3420"  },
    { "3480",   "3420"  },
    { "3490",   "3420"  },
    { "3590",   "3420"  },
    { "9347",   "3420"  },
    { "9348",   "3420"  },
    { "8809",   "3420"  },
    { "3422",   "3420"  },
    { "3430",   "3420"  },

    { "V3480",  "3480V" },
    { "V3490",  "3480V" },
    { "V3590",  "3480V" },

    { "DW3480",  "3590D" },
    { "DW3490",  "3590D" },
    { "DW3590",  "3590D" },
    { "TH3480",  "3590D" },
    { "TH3490",  "3590D" },
    { "TH3590",  "3590D" },

    { "8232C",  "8232"  },

    { "LCS",    "3088"  },
    { "CTCI",   "3088"  },
    { "CTCT",   "3088"  },
    { "VMNET",  "3088"  },

    { "HCHAN",  "2880"  },
//  { "2880",   "2880"  },
    { "2870",   "2880"  },
    { "2860",   "2880"  },
    { "9032",   "2880"  },

    { NULL,     NULL    } };


#if defined(OPTION_DYNAMIC_LOAD)
static char *hdt_device_type_equates(char *typname)
{
DTEQ *device_type;
char *(*nextcall)(char *);

    for(device_type = dteq; device_type->name; device_type++)
        if(!strcasecmp(device_type->alias, typname))
            return device_type->name;

    if((nextcall = HDL_FINDNXT(hdt_device_type_equates)))
        return nextcall(typname);

    return NULL;
}

/* Libtool static name colision resolution */
/* note : lt_dlopen will look for symbol & modulename_LTX_symbol */
/* for use in DLREOPEN case only */
#if !defined(HDL_BUILD_SHARED) && defined(HDL_USE_LIBTOOL)
#define hdl_ddev hdteq_LTX_hdl_ddev
#define hdl_depc hdteq_LTX_hdl_depc
#define hdl_reso hdteq_LTX_hdl_reso
#define hdl_init hdteq_LTX_hdl_init
#define hdl_fini hdteq_LTX_hdl_fini
#endif


HDL_DEPENDENCY_SECTION;
{
     HDL_DEPENDENCY(HERCULES);
}
END_DEPENDENCY_SECTION


HDL_REGISTER_SECTION;
{
    HDL_REGISTER(hdl_device_type_equates,hdt_device_type_equates);
}
END_REGISTER_SECTION
#endif
