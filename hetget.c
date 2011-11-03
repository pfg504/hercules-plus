/* HETGET.C     (c) Copyright Leland Lucius, 2000-2011               */
/*              (c) Copyright TurboHercules, SAS 2010-2011           */
/*              Extract files from an HET file                       */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

// $Id$

/*
|| ----------------------------------------------------------------------------
||
|| HETGET.C     (c) Copyright Leland Lucius, 2000-2009
||              Released under terms of the Q Public License.
||
|| Extract files from a AWS, HET or FAKETAPE file
||
|| ----------------------------------------------------------------------------
*/

#include "hstdinc.h"

#include "hercules.h"
#include "hetlib.h"
#include "ftlib.h"
#include "sllib.h"
#include "herc_getopt.h"

#define UTILITY_NAME    "hetget"

/*
|| Local volatile data
*/
  #define O_LIST      0x100
  #define O_NL        0x80
  #define O_ASCII     0x40
  #define O_STRIP     0x20
  #define O_UNBLOCK   0x10
  #define O_NO_NEW    0x08
  #define O_RDW       0x04
  #define O_HRCBIN    0x02
  #define O_HRCTXT    0x01

struct
{
    char *ifile;
    char *ofile;
    HETB *hetb;
    FETB *fetb;
    int faketape;
    int fileno;
    int lrecl;
    int blksize;
    unsigned int flags;
    unsigned char recfm;
}
opts = 
{
    NULL,
    NULL,
    NULL,
    NULL,
    FALSE,
    0,
    0,
    0,
    0,
    0,
};

/*
|| Valid record formats
*/
#define O_UNDEFINED 0x80
#define O_FIXED     0x40
#define O_VARIABLE  0x20
#define O_BLOCKED   0x08
#define O_SPANNED   0x04

static const struct
{
    char *recfm;
    int fmt;
}
valfm[] =
{
    { "U",      O_UNDEFINED | 0         | 0         },
    { "UA",     O_UNDEFINED | 0         | 0         },
    { "UM",     O_UNDEFINED | 0         | 0         },
    { "F",      O_FIXED     | 0         | 0         },
    { "FA",     O_FIXED     | 0         | 0         },
    { "FM",     O_FIXED     | 0         | 0         },
    { "FB",     O_FIXED     | O_BLOCKED | 0         },
    { "FBA",    O_FIXED     | O_BLOCKED | 0         },
    { "FBM",    O_FIXED     | O_BLOCKED | 0         },
    { "FS",     O_FIXED     | O_BLOCKED | 0         },
    { "FSA",    O_FIXED     | O_BLOCKED | 0         },
    { "FSM",    O_FIXED     | O_BLOCKED | 0         },
    { "FBS",    O_FIXED     | O_BLOCKED | 0         },
    { "FBSA",   O_FIXED     | O_BLOCKED | 0         },
    { "FBSM",   O_FIXED     | O_BLOCKED | 0         },
    { "V",      O_VARIABLE  | 0         | 0         },
    { "VA",     O_VARIABLE  | 0         | 0         },
    { "VM",     O_VARIABLE  | 0         | 0         },
    { "VB",     O_VARIABLE  | O_BLOCKED | 0         },
    { "VBA",    O_VARIABLE  | O_BLOCKED | 0         },
    { "VBM",    O_VARIABLE  | O_BLOCKED | 0         },
    { "VS",     O_VARIABLE  | 0         | O_SPANNED },
    { "VSA",    O_VARIABLE  | 0         | O_SPANNED },
    { "VSM",    O_VARIABLE  | 0         | O_SPANNED },
    { "VBS",    O_VARIABLE  | O_BLOCKED | O_SPANNED },
    { "VBSA",   O_VARIABLE  | O_BLOCKED | O_SPANNED },
    { "VBSM",   O_VARIABLE  | O_BLOCKED | O_SPANNED },
};
#define VALFMCNT ( sizeof( valfm ) / sizeof( valfm[ 0 ] ) )

/*
|| Block and record management
*/
unsigned char *blkptr = NULL;
int blkidx = 0;
int blklen = 0;
unsigned char *recptr = NULL;
int recidx = 0;
int reclen = 0;

#ifdef EXTERNALGUI
/*
|| Previously reported file position
*/
static off_t prevpos = 0;
/*
|| Report progress every this many bytes
*/
#define PROGRESS_MASK (~0x3FFFF /* 256K */)
#endif /*EXTERNALGUI*/

/*
|| Merge DCB information from HDR2 label
*/
void
merge( SLLABEL *lab, int disp_out )
{
    SLFMT fmt;
    int i;

    UNREFERENCED(disp_out);
    /*
    || Make the label more managable
    */
    sl_fmtlab( &fmt, lab );

    /*
    || Merge the record format;
    */
    if( opts.recfm == 0 )
    {
        opts.recfm = O_UNDEFINED;
        for( i = 0 ; i < (int)VALFMCNT ; i++ )
        {
            if( strcasecmp( fmt.slds2.recfm, valfm[ i ].recfm ) == 0 )
            {
                opts.recfm = valfm[ i ].fmt;
                break;
            }
        }
    }

    /*
    || Merge in the record length
    */
    if( opts.lrecl == 0 )
    {
        opts.lrecl = atoi( fmt.slds2.lrecl );
    }

    /*
    || Merge in the block size
    */
    if( opts.blksize == 0 )
    {
        /*
        || Try the blksize field first
        */
        opts.blksize = atoi( fmt.slds2.blksize );
        if( opts.blksize == 0 )
        {
            /*
            || Still zero, so try the lblkln field
            */
            opts.blksize = atoi( fmt.slds2.lblkln );
        }
    }

    /*
    || Locate final RECFM string
    */
    for( i = 0 ; i < (int)VALFMCNT ; i++ )
    {
        if( strcasecmp( fmt.slds2.recfm, valfm[ i ].recfm ) == 0 )
        {
            break;
        }
    }

// some of these functions are duplicated in hetmap.c
#if 0
    /*
    || Print DCB attributes
    */
    if ( disp_opt == 0 )
    {
        logmsg( "DCB Attributes used:\n" );
        logmsg( "  RECFM=%-4.4s  LRECL=%-5.5d  BLKSIZE=%d\n",
                valfm[ i ].recfm,
                opts.lrecl,
                opts.blksize );
    }
    else if ( disp_opt == 1 )
    {
        logmsg( " %-4.4s %5d %5d\n", valfm[ i ].recfm, opts.lrecl, opts.blksize );
    }
#endif

    return;
}

/*
|| Return block length from BDW
*/
int
bdw_length( const unsigned char *ptr  )
{
    unsigned int len;

    /*
    || Extended format BDW? 
    */
    if( ptr[ 0 ] & 0x80 )
    {
        /*
        || Length is 31 bits
        */
        len  = ptr[ 0 ] << 24;
        len += ptr[ 1 ] << 16;
        len += ptr[ 2 ] << 8;
        len += ptr[ 3 ];
        len &= 0x7fffffff;
    }
    else
    {
        /*
        || Length is 15 bits
        */
        len  = ptr[ 0 ] << 8;
        len += ptr[ 1 ];
    }

    return( len );
}

/*
|| Return record length from RDW
*/
int
rdw_length( const unsigned char *ptr )
{
    unsigned int len;

    /*
    || Get the record length
    */
    len  = ptr[ 0 ] << 8;
    len += ptr[ 1 ];

    return( len );
}

/*
|| Retrieves a block from the tape file and resets variables
*/
int
getblock( )
{
    int rc;

    /*
    || Read a block from the tape
    */
    if ( opts.faketape )
        rc = fet_read( opts.fetb, blkptr );
    else
        rc = het_read( opts.hetb, blkptr );
    if( rc < 0 )
    {
        return( rc );
    }

    /*
    || Save the block length, and use BDW for RECFM=V to protect
       against minimum block sizes (which avoid tape noise).
    */
    blklen = rc;
    if( (blklen > 4) && (opts.recfm & O_VARIABLE) )
    {
        int bdw;

        bdw = bdw_length( blkptr );
        if ( (bdw > 4) && (bdw < blklen) )
        {
            blklen = bdw;
        }
    }

    rc = blklen;

    return( rc );
}

/*
|| Retrieve logical records from the tape - doesn't handle SPANNED records
*/
int
getrecord( )
{
    int rc;
    
    /*
    || Won't be null if we've been here before
    */
    if( recptr != NULL )
    {
        recidx += reclen;
    }

    /*
    || Need a new block first time through or we've exhausted current block
    */
    if( ( recptr == NULL ) || ( recidx >= blklen ) )
    {
        /*
        || Go get another block
        */
        rc = getblock( );
        if( rc < 0 )
        {
            return( rc );
        }
    
        /*
        || For RECFM=V, bump index past BDW
        */
        recidx = 0;
        if( opts.recfm & O_VARIABLE )
        {
            /* protect against a corrupt (short) block */
            if ( rc < 8 )
            {
                return ( -1 );
            }
            recidx = 4;
        }
    }

    /*
    || Set the new record pointer
    */
    recptr = &blkptr[ recidx ];

    /*
    || Set the record length depending on record type
    */
    if( opts.recfm & O_FIXED )
    {
        reclen = opts.lrecl;
    }
    else if( opts.recfm & O_VARIABLE )
    {
        reclen = rdw_length( recptr );

        /* protect against corrupt (short) block */
        if ( reclen + recidx > blklen )
        {
            return (-1);
        }
        /* protect against a corrupt (less than 4) RDW */
        if ( reclen < 4 )
        {
            return (-1);
        }
    }
    else
    {
        reclen = blklen;
    }

    return( reclen );
}

/*
|| Retrieve and validate a standard label
*/
int
get_sl( SLLABEL *lab )
{
    int rc;
    
    /*
    || Read a block
    */
    if ( opts.faketape )
        rc = fet_read( opts.fetb, blkptr );
    else
        rc = het_read( opts.hetb, blkptr );
    if( rc >= 0 )
    {
        /*
        || Does is look like a standard label?
        */
        if( sl_islabel( lab, blkptr, rc ) == TRUE )
        {
            return( 0 );
        }
    }
    else
    {
        if ( opts.faketape )
            WRMSG( HHC00075, "E", "fet_read()", fet_error( rc ) );
        else
            WRMSG( HHC00075, "E", "het_read()", het_error( rc ) );
    }

    return( -1 );
}

/*
|| List files in the tape "archive" (ie think "unzip -v")
   Only works on SL tapes.
*/
int
listfiles( )
{
    SLFMT fmt;
    SLLABEL lab;
    int rc;

    /*
    || First block should be a VOL1 record
    */
    rc = get_sl( &lab );
    if( rc < 0 || !sl_isvol( &lab, 1 ) )
    {
        logmsg( "Expected VOL1 label\n" );
        return( -1 );
    }
        
    /* process all files on tape */
    while (rc >= 0)
    {
        /*
        || Get the HDR1 label.
        */
        rc = get_sl( &lab );
        if( rc < 0 || !sl_ishdr( &lab, 1 ) )
        {
            /* quietly return when no more files */
            return( 0 );
        }

        /*
        || Make the label more managable
        */
        sl_fmtlab( &fmt, &lab );
        logmsg("%-17.17s", fmt.slds1.dsid ); 

        /*
        || Get the HDR2 label.
        */
        rc = get_sl( &lab );
        if( rc < 0 || !sl_ishdr( &lab, 2 ) )
        {
            logmsg( "Expected HDR2 label\n" );
            return( -1 );
        }

        /*
        || Merge the DCB information
        */
        merge( &lab, 1 );

        /*
        || Hop over the tapemark
        */
        if ( opts.faketape )
            rc = fet_fsf( opts.fetb );
        else
            rc = het_fsf( opts.hetb );
        if( rc < 0 )
        {
            logmsg( "%s while spacing to start of data\n",
                het_error( rc ) );
            return( rc );
        }

        /*
        || skip data file, if any
        */
        if ( opts.faketape )
            rc = fet_fsf( opts.fetb );
        else
            rc = het_fsf( opts.hetb );
        if( rc < 0 )
        {
            return( 0 );
        }
        /*
        || skip EOF file, if any
        */
        if ( opts.faketape )
            rc = fet_fsf( opts.fetb );
        else
            rc = het_fsf( opts.hetb );
        if( rc < 0 )
        {
            return( 0 );
        }

    } /* while */
    return (0);
}

/*
|| Extract files from the tape "archive" (ie think "unzip")
   Only works on SL tapes.
*/
int
extractfiles( )
{
    SLFMT fmt;
    SLLABEL lab;
    unsigned char *ptr;
    int rc;
    FILE *outf;
    
    /*
    || First block should be a VOL1 record
    */
    rc = get_sl( &lab );
    if( rc < 0 || !sl_isvol( &lab, 1 ) )
    {
        logmsg( "Expected VOL1 label\n" );
        return( -1 );
    }
        
    /* process all files on tape */
    while (rc >= 0)
    {
        /*
        || Get the HDR1 label.
        */
        rc = get_sl( &lab );
        if( rc < 0 || !sl_ishdr( &lab, 1 ) )
        {
            /* quietly return when no more files */
            return( 0 );
        }

        /*
        || Make the label more managable
        */
        sl_fmtlab( &fmt, &lab );
        logmsg("%-17.17s", fmt.slds1.dsid ); 
    
        /*
        || Get the HDR2 label.
        */
        rc = get_sl( &lab );
        if( rc < 0 || !sl_ishdr( &lab, 2 ) )
        {
            logmsg( "Expected HDR2 label\n" );
            return( -1 );
        }
    
        /*
        || Merge the DCB information
        */
        merge( &lab, 1 );

        /*
        || Hop over the tapemark
        */
        if ( opts.faketape )
            rc = fet_fsf( opts.fetb );
        else
            rc = het_fsf( opts.hetb );
        if( rc < 0 )
        {
            logmsg( "%s while spacing to start of data\n",
                het_error( rc ) );
            return( rc );
        }

        /*
        || process the current file
        */
        {
            /*
            || Open the output file
            */
            char pathname[MAX_PATH];
            
            opts.ofile = fmt.slds1.dsid;
            hostpath(pathname, opts.ofile, sizeof(pathname));
            outf = fopen( pathname, (opts.flags & O_ASCII) ? "w" : "wb" );
            if( outf == NULL )
            {
                logmsg("unable to open %s\n", opts.ofile);
                return( -1 );
            }
        }








    /* this should be in a common block, or at least indented */

    /*
    || Different processing when converting to ASCII
    */
    if( opts.flags & ( O_ASCII | O_UNBLOCK | O_RDW ) )
    {
        /*
        || Get a record
        */
        while( ( rc = getrecord( ) ) >= 0 )
        {
#ifdef EXTERNALGUI
            if( extgui )
            {
                off_t curpos;
                /* Report progress every nnnK */
                if ( opts.faketape )
                     curpos = ftell( opts.fetb->fd );
                else
                     curpos = ftell( opts.hetb->fd );
                if( ( curpos & PROGRESS_MASK ) != ( prevpos & PROGRESS_MASK ) )
                {
                    prevpos = curpos;
                    fprintf( stderr, "IPOS=%" I64_FMT "d\n", (U64)curpos );
                }
            }
#endif /*EXTERNALGUI*/
            /*
            || Get working copy of record ptr
            */
            ptr = recptr;

            /*
            || Only want data portion for RECFM=V records
            */
            if( opts.recfm & O_VARIABLE )
            {
                ptr += 4;
                rc -= 4;
            }

            /*
            || Convert record to ASCII
            */
            if( opts.flags & O_ASCII )
            {
                sl_etoa( NULL, ptr, rc );
            }

            /*
            || Strip trailing blanks
            */
            if( opts.flags & O_STRIP 
                || ((opts.flags & O_HRCTXT)
                    && (opts.recfm & O_FIXED)
                   )
              )
            {
                while( rc > 0 && ptr[ rc - 1 ] == ' ' )
                {
                    rc--;
                }
                
                /* if a text file has been copied, in binary mode,
                   into a fixed dataset, it will have NUL-padding.
                   Since we don't want NULs in a text file, we
                   clean them up too */
                if (opts.recfm & O_FIXED)
                {
                    while( rc > 0 && ptr[ rc - 1 ] == '\0' )
                    {
                        rc--;
                    }
                }
            }
            
            /*
            || Write the record out
            */
            if ( (opts.flags & O_ASCII)
                 && rc == 1
                 && ptr[0] == ' '
                 && !(opts.flags & O_RDW)
                 && ( ((opts.recfm & O_UNDEFINED)
                       && !(opts.flags & O_NO_NEW)
                      )
                      || (opts.recfm & O_VARIABLE)
                    )
               )
            {
                /* if the dataset is undefined or variable and has a 
                   single space, then don't write out that space,
                   because the space most likely exists because it
                   was artificially inserted to prevent empty
                   records or blocks rather than because the user
                   really wants a space. Also, if they are taking
                   care of newlines themselves for RECFM=U, then
                   any single space in the last block would be
                   genuine albeit extremely unlikely. */
                rc = 0;
            }
            
            /* write out an artificial RDW */
            if ((opts.flags & O_RDW)
                || ((opts.flags & O_HRCBIN)
                    && (opts.recfm & O_VARIABLE)
                   )
               )
            {
                int havenl = 0;

                /* take into account newline */
                if( opts.flags & O_ASCII 
                    && (!(opts.flags & O_NO_NEW) 
                        || !(opts.recfm & O_UNDEFINED)
                       )
                  )
                {
                    havenl = 1;
                    rc++;
                }
                rc += 4;
                fputc( (((unsigned int)rc >> 8) & 0xff), outf );
                fputc( ((unsigned int)rc & 0xff), outf );
                fputc( 0x00, outf );
                fputc( 0x00, outf );
                rc -= 4;
                if (havenl)
                {
                    rc--;
                }
            }
            fwrite( ptr, rc, 1, outf );

            /*
            || Put out a linefeed when converting
            */
            if( opts.flags & O_ASCII 
                && (!(opts.flags & O_NO_NEW) 
                    || !(opts.recfm & O_UNDEFINED)
                   )
              )
            {
                fwrite( "\n", 1, 1, outf );
            }
        }
    }
    else
    {
        /*
        || Get a record
        */
        while( ( rc = getblock( ) ) >= 0 )
        {
#ifdef EXTERNALGUI
            if( extgui )
            {
                off_t curpos;
                /* Report progress every nnnK */
                if ( opts.faketape )
                     curpos = ftell( opts.fetb->fd );
                else
                     curpos = ftell( opts.hetb->fd );
                if( ( curpos & PROGRESS_MASK ) != ( prevpos & PROGRESS_MASK ) )
                {
                    prevpos = curpos;
                    fprintf( stderr, "IPOS=%" I64_FMT "d\n", (U64)curpos );
                }
            }
#endif /*EXTERNALGUI*/
            /*
            || Write the record out
            */
            fwrite( blkptr, blklen, 1, outf );
        }
    }

        fclose(outf); /* finished writing a single file */

        /*
        || skip EOF file, if any
        */
        if ( opts.faketape )
            rc = fet_fsf( opts.fetb );
        else
            rc = het_fsf( opts.hetb );
        if( rc < 0 )
        {
            return( 0 );
        }

    } /* while */
    return (0);
}


/*
|| Extract the file from the tape
*/
int
getfile( FILE *outf )
{
    SLFMT fmt;
    SLLABEL lab;
    unsigned char *ptr;
    int fileno;
    int rc;
    
    /*
    || Skip to the desired file
    */
    if( opts.flags & O_NL )
    {
        /*
        || For NL tapes, just use the specified file number
        */
        fileno = opts.fileno;

        /*
        || Start skipping
        */
        while( --fileno )
        {
            /*
            || Forward space to beginning of next file
            */
            if ( opts.faketape )
                rc = fet_fsf ( opts.fetb );
            else
                rc = het_fsf( opts.hetb );
            if( rc < 0 )
            {
                char msgbuf[128];
                MSGBUF( msgbuf, "%set_fsf() while positioning to file '%d'", 
                    opts.faketape ? "f" : "h",
                    opts.fileno ); 
                WRMSG( HHC00075, "E", msgbuf, het_error( rc ) );
                return( rc );
            }
        }
    }
    else
    {
        /*
        || First block should be a VOL1 record
        */
        rc = get_sl( &lab );
        if( rc < 0 || !sl_isvol( &lab, 1 ) )
        {
            WRMSG( HHC02753, "E", "VOL1" );
            return( -1 );
        }

        /*
        || For SL, adjust the file # so we end up on the label before the data
        */
        fileno = ( opts.fileno * 3 ) - 2;

        /*
        || Start skipping
        */
        while( --fileno )
        {
            /*
            || Forward space to beginning of next file
            */
            if ( opts.faketape )
                rc = fet_fsf ( opts.fetb );
            else
                rc = het_fsf( opts.hetb );
            if( rc < 0 )
            {
                char msgbuf[128];
                MSGBUF( msgbuf, "%set_fsf() while positioning to file '%d'", 
                    opts.faketape ? "f" : "h",
                    opts.fileno ); 
                WRMSG( HHC00075, "E", msgbuf, het_error( rc ) );
                return( rc );
            }
        }

        /*
        || Get the HDR1 label.
        */
        rc = get_sl( &lab );
        if( rc < 0 || !sl_ishdr( &lab, 1 ) )
        {
            WRMSG( HHC02753, "E", "HDR1" );
            return( -1 );
        }

        /*
        || Make the label more managable
        */
        sl_fmtlab( &fmt, &lab );
        WRMSG( HHC02754, "E", fmt.slds1.dsid ); 
    
        /*
        || Get the HDR2 label.
        */
        rc = get_sl( &lab );
        if( rc < 0 || !sl_ishdr( &lab, 2 ) )
        {
            WRMSG( HHC02753, "E", "HDR2" );
            return( -1 );
        }
    
        /*
        || Merge the DCB information
        */
        merge( &lab, 0 );

        /*
        || Hop over the tapemark
        */
        if ( opts.faketape )
            rc = fet_fsf( opts.fetb );
        else
            rc = het_fsf( opts.hetb );
        if( rc < 0 )
        {
            if ( opts.faketape )
                WRMSG( HHC00075, "E", "fet_fsf()", fet_error( rc ) );
            else
                WRMSG( HHC00075, "E", "het_fsf()", het_error( rc ) );
            return( rc );
        }
    }

    /*
    || Different processing when converting to ASCII
    */
    if( opts.flags & ( O_ASCII | O_UNBLOCK | O_RDW ) )
    {
        /*
        || Get a record
        */
        while( ( rc = getrecord( ) ) >= 0 )
        {
#ifdef EXTERNALGUI
            if( extgui )
            {
                off_t curpos;
                /* Report progress every nnnK */
                if ( opts.faketape )
                    curpos = ftell( opts.fetb->fd ); 
                else
                    curpos = ftell( opts.hetb->fd );
                if( ( curpos & PROGRESS_MASK ) != ( prevpos & PROGRESS_MASK ) )
                {
                    prevpos = curpos;
                    fprintf( stderr, "IPOS=%" I64_FMT "d\n", (U64)curpos );
                }
            }
#endif /*EXTERNALGUI*/
            /*
            || Get working copy of record ptr
            */
            ptr = recptr;

            /*
            || Only want data portion for RECFM=V records
            */
            if( opts.recfm & O_VARIABLE )
            {
                ptr += 4;
                rc -= 4;
            }

            /*
            || Convert record to ASCII
            */
            if( opts.flags & O_ASCII )
            {
                sl_etoa( NULL, ptr, rc );
            }

            /*
            || Strip trailing blanks
            */
             if( opts.flags & O_STRIP || ( (opts.flags & O_HRCTXT) &&
                                           (opts.recfm & O_FIXED) )
               )
             {
                while( rc > 0 && ptr[ rc - 1 ] == ' ' )
                {
                    rc--;
                }

                /* if a text file has been copied, in binary mode,
                   into a fixed dataset, it will have NUL-padding.
                   Since we don't want NULs in a text file, we
                   clean them up too */
                if (opts.recfm & O_FIXED)
                {
                    while( rc > 0 && ptr[ rc - 1 ] == '\0' )
                    {
                        rc--;
                    }
                }
             }

            /*
            || Write the record out
            */
            if ( (opts.flags & O_ASCII)
                 && rc == 1
                 && ptr[0] == ' '
                 && !(opts.flags & O_RDW)
                 && ( ((opts.recfm & O_UNDEFINED)
                       && !(opts.flags & O_NO_NEW)
                      )
                      || (opts.recfm & O_VARIABLE)
                    )
               )
            {
                /* if the dataset is undefined or variable and has a 
                   single space, then don't write out that space,
                   because the space most likely exists because it
                   was artificially inserted to prevent empty
                   records or blocks rather than because the user
                   really wants a space. Also, if they are taking
                   care of newlines themselves for RECFM=U, then
                   any single space in the last block would be
                   genuine albeit extremely unlikely. */
                rc = 0;
            }
            
            /* write out an artificial RDW */
            if ((opts.flags & O_RDW)
                || ((opts.flags & O_HRCBIN)
                    && (opts.recfm & O_VARIABLE)
                   )
               )
            {
                int havenl = 0;

                /* take into account newline */
                if( opts.flags & O_ASCII 
                    && (!(opts.flags & O_NO_NEW) 
                        || !(opts.recfm & O_UNDEFINED)
                       )
                  )
                {
                    havenl = 1;
                    rc++;
                }
                rc += 4;
                fputc( (((unsigned int)rc >> 8) & 0xff), outf );
                fputc( ((unsigned int)rc & 0xff), outf );
                fputc( 0x00, outf );
                fputc( 0x00, outf );
                rc -= 4;
                if (havenl)
                {
                    rc--;
                }
            }
             fwrite( ptr, rc, 1, outf );

            /*
            || Put out a linefeed when converting
            */
            if( opts.flags & O_ASCII 
                && (!(opts.flags & O_NO_NEW) 
                    || !(opts.recfm & O_UNDEFINED)
                   )
              )
            {
                fwrite( "\n", 1, 1, outf );
            }
        }
    }
    else
    {
        /*
        || Get a record
        */
        while( ( rc = getblock( ) ) >= 0 )
        {
#ifdef EXTERNALGUI
            if( extgui )
            {
                off_t curpos;
                /* Report progress every nnnK */
                if ( opts.faketape )
                    curpos = ftell( opts.fetb->fd );
                else
                    curpos = ftell( opts.hetb->fd );
                if( ( curpos & PROGRESS_MASK ) != ( prevpos & PROGRESS_MASK ) )
                {
                    prevpos = curpos;
                    fprintf( stderr, "IPOS=%" I64_FMT "d\n", (U64)curpos );
                }
            }
#endif /*EXTERNALGUI*/
            /*
            || Write the record out
            */
            fwrite( blkptr, blklen, 1, outf );
        }
    }

    return( rc );
}

/*
|| Prints usage information
*/
void
usage( char *name )
{
    logmsg( MSG( HHC02728, "I", name ) );
}

/*
|| Standard main
*/
int
main( int argc, char *argv[] )
{
    char           *pgmname;                /* prog name in host format  */
    char           *pgm;                    /* less any extension (.ext) */
    char            msgbuf[512];            /* message build work area   */
    FILE           *outf;
    int             rc;
    int             i;
    char            pathname[MAX_PATH];
    char           *strtok_str = NULL;

    /* Set program name */
    if ( argc > 0 )
    {
        if ( strlen(argv[0]) == 0 )
        {
            pgmname = strdup( UTILITY_NAME );
        }
        else
        {
            char path[MAX_PATH];
#if defined( _MSVC_ )
            GetModuleFileName( NULL, path, MAX_PATH );
#else
            strncpy( path, argv[0], sizeof( path ) );
#endif
            pgmname = strdup(basename(path));
#if !defined( _MSVC_ )
            strncpy( path, argv[0], sizeof(path) );
#endif
        }
    }
    else
    {
        pgmname = strdup( UTILITY_NAME );
    }

    pgm = strtok_r( strdup(pgmname), ".", &strtok_str);
    INITIALIZE_UTILITY( pgmname );

    /* Display the program identification message */
    MSGBUF( msgbuf, MSG_C( HHC02499, "I", pgm, "Extract Files from AWS, HET or FAKETAPE" ) );
    display_version (stderr, msgbuf+10, FALSE);

    /*
    || Process option switches
    */
    while( TRUE )
    {
        rc = getopt( argc, argv, "abhlnsturz" );
        if( rc == -1 )
        {
            break;
        }

        switch( rc )
        {
            case 'a':
                opts.flags |= O_ASCII;
                set_codepage(NULL);
            break;

            case 'b':
                opts.flags |= O_HRCBIN;
                opts.flags |= O_UNBLOCK;
            break;

            case 'h':
                usage( pgm );
                exit( 1 );
            break;

            case 'l':
                opts.flags |= O_LIST;
            break;

            case 'n':
                opts.flags |= O_NL;
            break;

            case 's':
                opts.flags |= O_STRIP;
            break;

            case 't':                
                opts.flags |= O_HRCTXT;
                opts.flags |= O_ASCII;
                opts.flags |= O_UNBLOCK;
                opts.flags |= O_NO_NEW;
            break;

            case 'u':
                opts.flags |= O_UNBLOCK;
            break;

            case 'r':
                opts.flags |= O_RDW;
            break;

            case 'z':
                opts.flags |= O_NO_NEW;
            break;

            default:
                usage( pgm );
                exit( 1 );
            break;
        }
    }

    /*
    || Calc number of non-switch arguments
    */
    argc -= optind;

    if ( opts.flags & O_LIST )
    {
        hostpath( pathname, argv[ optind ], sizeof(pathname) );
        opts.ifile = strdup( pathname );
        if ( ( rc = (int)strlen( opts.ifile ) ) > 4 
            && ( rc = strcasecmp( &opts.ifile[rc-4], ".fkt" ) ) == 0 )
        {
            opts.faketape = TRUE;
        }
        blkptr = (BYTE*)malloc( HETMAX_BLOCKSIZE );
        if ( blkptr != NULL )
        {
            if ( opts.faketape )
            {
                rc = fet_open( &opts.fetb, opts.ifile, FETOPEN_READONLY );
                if ( rc >= 0 )
                {
                    listfiles( );
                    fet_close( &opts.fetb );
                }
            }
            else
            {
                rc = het_open( &opts.hetb, opts.ifile, HETOPEN_READONLY );
                if ( rc >= 0 )
                {
                    listfiles( );
                    het_close( &opts.hetb );
                }
            }
            return 0;
        }
        else
            return -1;
    }

    if ( argc == 1 )
    {
        /* unless otherwise specified (via a deliberate
           setting of the text options), assume they want
           binary with sensible defaults */
        if (!(opts.flags & O_ASCII))
        {
            opts.flags |= O_HRCBIN;
            opts.flags |= O_UNBLOCK;
        }
        hostpath( pathname, argv[ optind ], sizeof(pathname) );
        opts.ifile = strdup( pathname );
        if ( ( rc = (int)strlen( opts.ifile ) ) > 4 
            && ( rc = strcasecmp( &opts.ifile[rc-4], ".fkt" ) ) == 0 )
        {
            opts.faketape = TRUE;
        }
        blkptr = (BYTE*)malloc( HETMAX_BLOCKSIZE );
        if ( blkptr != NULL )
        {
            if ( opts.faketape )
            {
                rc = fet_open( &opts.fetb, opts.ifile, FETOPEN_READONLY );
                if ( rc >= 0 )
                {
                    extractfiles( );
                    fet_close( &opts.fetb );
                }
            }
            else
            {
                rc = het_open( &opts.hetb, opts.ifile, HETOPEN_READONLY );
                if ( rc >= 0 )
                {
                    extractfiles( );
                    het_close( &opts.hetb );
                }
            }
            return 0;
        }
        else
            return -1;
    }
    /*
    || We must have at least the first 3 parms
    */
    if(argc < 3)
    {
        if ( argc > 1 )
            WRMSG( HHC02446, "E" );
        usage( pgm );
        exit( 1 );
    }

    hostpath( pathname, argv[ optind ], sizeof(pathname) );
    opts.ifile = strdup( pathname );
    if ( ( rc = (int)strlen( opts.ifile ) ) > 4 
      && ( rc = strcasecmp( &opts.ifile[rc-4], ".fkt" ) ) == 0 )
    {
        opts.faketape = TRUE;
    }

    hostpath( pathname, argv[ optind + 1 ], sizeof(pathname) );
    opts.ofile = strdup( pathname );

    opts.fileno = atoi( argv[ optind + 2 ] );

    if( opts.fileno == 0 || opts.fileno > 9999 )
    {
        char msgbuf[20];
        MSGBUF( msgbuf, "%d", opts.fileno );
        WRMSG( HHC02205, "E", msgbuf, "; file number must be within the range of 1 to 9999" );
        exit( 1 );
    }

    /*
    || If NL tape, then we require the DCB attributes
    */
    if( opts.flags & O_NL )
    {
        if( argc != 6 )
        {
            WRMSG( HHC02750, "E" );
            exit( 1 );
        }
    }

    /*
    || If specified, get the DCB attributes
    */
    if( argc > 3 )
    {
        /*
        || Must have only three
        */
        if( argc != 6 )
        {
            usage( pgm );
            exit( 1 );
        }

        /*
        || Lookup the specified RECFM in our table
        */
        opts.recfm = 0;
        for( i = 0 ; i < (int)VALFMCNT ; i++ )
        {
            if( strcasecmp( argv[ optind + 3 ], valfm[ i ].recfm ) == 0 )
            {
                opts.recfm = valfm[ i ].fmt;
                break;
            }
        }

        /*
        || If we didn't find a match, show the user what the valid ones are
        */
        if( opts.recfm == 0)
        {
            char msgbuf[512] = "";
            char msgbuf2[64] = "";
            char msgbuf3[16] = "";
            char msgbuf4[128] = "";

            /*
            || Dump out the valid RECFMs
            */
            MSGBUF( msgbuf, MSG( HHC02751, "I" ) );
            for( i = 0 ; i < (int)VALFMCNT ; i++ )
            {
                MSGBUF( msgbuf3, "  %-4.4s", valfm[ i ].recfm );

                if( ( ( i + 1 ) % 3 ) == 0 )
                {
                    strcat( msgbuf2, msgbuf3 );
                    MSGBUF( msgbuf4, MSG( HHC02752, "I", msgbuf2 ) );
                    strcat( msgbuf, msgbuf4 );
                    msgbuf2[0] = 0;
                }
                else
                {
                    strcat( msgbuf2, msgbuf3 );
                }
            }
            logmsg( "%s", msgbuf );
            exit( 1 );
        }

        /*
        || Get the record length
        */
        opts.lrecl = atoi( argv[ optind + 4 ] );

        /*
        || Get and validate the blksize
        */
        opts.blksize = atoi( argv[ optind + 5 ] );
        if( opts.blksize == 0 )
        {
            WRMSG( HHC02205, "E", "0", "; block size can't be zero" );
            exit( 1 );
        }
    }

    /*
    || Open the tape file
    */
    if ( opts.faketape )
        rc = fet_open( &opts.fetb, opts.ifile, FETOPEN_READONLY );
    else
        rc = het_open( &opts.hetb, opts.ifile, HETOPEN_READONLY );
    if( rc >= 0 )
    {
        /*
        || Get memory for the tape buffer
        */
        blkptr = malloc( HETMAX_BLOCKSIZE );
        if( blkptr != NULL )
        {
            /*
            || Open the output file
            */
            outf = fopen( opts.ofile, (opts.flags & O_ASCII) ? "w" : "wb" );
            if( outf != NULL )
            {
                /*
                || Go extract the file from the tape
                */
                rc = getfile( outf );

                /*
                || Close the output file
                */
                fclose( outf );
            }
            
            /*
            || Free the buffer memory
            */
            free( blkptr );
        }
    }
    else
    {
        if ( opts.faketape )
            WRMSG( HHC00075, "E", "fet_open()", fet_error( rc ) );
        else
            WRMSG( HHC00075, "E", "het_open()", het_error( rc ) );
    }

    /*
    || Close the tape file
    */
    if ( opts.faketape )
        fet_close( &opts.fetb );
    else
        het_close( &opts.hetb );

    return 0;
}
