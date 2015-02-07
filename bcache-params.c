/*
 * Author: Rolf Fokkens <rolf@rolffokkens.nl>
 *
 * GPLv2
 *
 * For experimenting (or production) it may be useful to set bcache
 * parameters in an early stage during boot, for example to tune the
 * boot performance when the root fs is on a bcache device. The best
 * moment is right before the root fs is actually mounted, which means
 * it may need to be done in the initramfs.
 *
 * The bcache kernel driver does not support passing kernel cmdline
 * arguments to it. This udev helper can be excuted from udev rules to
 * take care of cmdline arguments by changing bcache parameters using
 * the /sys interface right after a bcache device is brought up. This
 * works both in the initramfs and later.
 *
 * It recognizes cmdline arguments like these:
 *   bcache=sco:0,crdthr:0,cache/congested_write_threshold_us:0
 * This means:
 * - for any bcache device set the following parameters:
 * - sequential_cutoff (sco) is set to 0
 * - cache/congested_read_threshold_us (crdthr) is set to 0
 * - cache/congested_write_threshold_us (cwrthr) is set to 0
 * Both short aliases (for user convenience) and full parameters can be used,
 * they are defined in the parm_map below.
 *
 * Other parameters are not accepted, because they're not useful or 
 * potentially harmful (e.g. changing the label, stopping bcache devices)
 *
 * Parsing of each kernel cmdline argument is done in a simple way:
 * - does the argument start with "bcache="? If not: next argument.
 * - for the rest of the argument, identify "subarguments":
 *   - is what follows of the form <stringP>:<stringV>? If not: next argument
 *   - is <stringP> a know parameter name? If not: next subargument
 *   - process the subargument
 *   - next subargument
 */

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <error.h>
#include <errno.h>

#ifndef TESTING
#   define CMDLINE "/proc/cmdline"
#   define SYSPFX "/sys/block"
#else
#   define CMDLINE "/tmp/cmdline"   
#   define SYSPFX "/tmp"            
#endif

struct parm_map {
    char *id;
    char *full;
};

/*
 * The list of kernel cmdline patameters that can be processed by 
 * bcache-patams.
 */
struct parm_map parm_map[] = {
    { "crdthr",                       "cache/congested_read_threshold_us"  }
,   { "cwrthr",                       "cache/congested_write_threshold_us" }
,   { "rdahed",                       "readahead"                          }
,   { "sctoff",                       "sequential_cutoff"                  }
,   { "wrbdly",                       "writeback_delay"                    }
,   { "wrbpct",                       "writeback_percent"                  }
,   { "wrbupd",                       "writeback_rate_update_seconds"      }
,   { NULL                          , NULL                                 }
};

/*
 * Read characters from fp (/proc/cmdline) into buf until maxlen characters are
 * read or until a character is read that is in the list of terminators.
 *
 * lookaheadp points to the current lookahead symbol, and is returned as such to
 * the caller.
 *
 * When a characters is read that is a terminator charachter, the lookehead is moved
 * one character ahead and the encountered terminator is returned.
 *
 * If for another reason reading characters stops, 0 is returned.
 */
int read_until (int *lookaheadp, FILE *fp, char *buf, int maxlen, char *terminators)
{
    char *cp = buf, *ep = buf + maxlen;
    int lookahead = *lookaheadp;

    while (   cp < ep
           && lookahead != EOF
           && isprint (lookahead)
           && !strchr (terminators, lookahead)) {
        *cp++ = lookahead;
        lookahead = fgetc (fp);
    }

    *lookaheadp = lookahead;

    *cp = '\0';

    if (strchr (terminators, lookahead)) {
        *lookaheadp = fgetc (fp);
        return lookahead;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    char buf[256];
    int  la, bufsz = sizeof (buf);
    FILE *fp;

    if (argc != 2) {
        fprintf (stderr, "bcache-params takes exactly one argument\n");
        return 1;
    }

    fp = fopen (CMDLINE, "r");
    if (fp == NULL) {
        perror ("Error opening /proc/cmdline");
        return 1;
    }
    /* la is our lookahead character */
    la = fgetc (fp);

    while (la != EOF) {
        /* skip any spaces */
        while (la == ' ') {
            la = fgetc (fp);
        }
        /* stop ehen end of the line */
        if (la == EOF) break;

        /* process until '=' */
        if (read_until (&la, fp, buf, bufsz, " =") != '=') goto nextarg;
        /* did we get a "bcache=" prefix? */
        if (strcmp (buf, "bcache") != 0) goto nextarg;

        /* process subarguments */
        for (;;) {
            struct parm_map *pmp;
            char sysfile[256];
            int term, fd;

            /* all subargs start with "<string>:" */
            if (read_until (&la, fp, buf, bufsz, " :") != ':') goto nextarg;

            /* now identify <string> */
            for (pmp = parm_map; pmp->id != NULL; pmp++) {
                if (strcmp (buf, pmp->id)   == 0) break;
                if (strcmp (buf, pmp->full) == 0) break;
            }
            /* no match for <string>? next subargument */
            if (pmp->id == NULL) {
                error (0, 0, "Unknown bcache parameter %s", buf);
            } else {
                /* Now we know what /sys file to write to */
                sprintf (sysfile, "%s/%s/bcache/%s", SYSPFX, argv[1], pmp->full);
             }

            /* What follows is the data to be written */
            term = read_until (&la, fp, buf, bufsz, " ,");

            if (pmp->id == NULL) goto nextsubarg;

            /* no data identified? next subargument */
            if (buf[0] == '\0') {
                error (0, 0, "Missing data for parameter %s(%s)", pmp->full, pmp->id);
                goto nextsubarg;
            }
            /* we're ready to actually write the data */
            fd = open (sysfile, O_WRONLY);
            if (fd < 0) {
                error (0, errno, "Error opening %s", sysfile);
                goto nextsubarg;
            }
            if (dprintf (fd, "%s\n", buf) < 0) {
                error (0, errno, "Error writing %s to %s", buf, sysfile);
            }
            close (fd);
            /* From here there's the hope for another subargument */
        nextsubarg:
            if (term != ',') break;
        }
    nextarg:
        while (la != EOF && la != ' ') la = fgetc (fp);
    }
    return 0;
}

