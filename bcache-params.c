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
 * The bcache kernel drivers doe snot support passing kernel cmdline
 * parameters to it. This program can be excuted from udev rules to
 * take care of it by changing bcache parameters using the /sys interface
 * right after a bcache device is brought up. This works both in the
 * initramfs and later.
 *
 * It recognizes parameters like these:
 *   bcache=sco:0,crdthr:0,cache/congested_write_threshold_us:0
 * This means:
 * - parameters are set for any bcache device
 * - sequential_cutoff (sco) is set to 0
 * - cache/congested_read_threshold_us (crdthr) is set to 0
 * - cache/congested_write_threshold_us (cwrthr) is set to 0
 * Both short aliases (for user convenience) and full parameters can be used:
 *
 * sco:    sequential_cutoff
 * crdthr: cache/congested_read_threshold_us
 * cwrthr: cache/congested_write_threshold_us
 *
 * Other parameters are not accepted, because they're not useful or 
 * potentially harmful (e.g. changing the label, stopping bcache devices)
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <error.h>
#include <errno.h>

#define CMDLINE "/proc/cmdline"
#define SYSPFX "/sys/block"

/* #define CMDLINE "/tmp/cmdline"   */
/* #define SYSPFX "/tmp"            */

struct parm_map {
    char *id;
    char *full;
};

struct parm_map parm_map[] = {
    { "sco",                          "sequential_cutoff"                  }
,   { "crdthr",                       "cache/congested_read_threshold_us"  }
,   { "cwrthr",                       "cache/congested_write_threshold_us" }
,   { NULL                          , NULL                                 }
};

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
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int  c, fd;
    FILE *fp;
    char buf[256];
    const char prefix[] = "bcache";

    if (argc != 2) {
        fprintf (stderr, "bcache-params takes exactly one argument\n");
        return 1;
    }

    fp = fopen (CMDLINE, "r");
    if (fp == NULL) {
        perror ("Error opening /proc/cmdline");
        return 1;
    }
    c = fgetc (fp);

    while (c != EOF) {
        /* skip any spaces */
        while (c == ' ') {
            c = fgetc (fp);
        }
        /* stop ehen end of the line */
        if (c == EOF) break;

        if (!read_until (&c, fp, buf, sizeof (buf), " =")) goto skiprest;
        if (strcmp (buf, prefix) != 0) goto skiprest;

        for (;;) {
            struct parm_map *pmp;
            char sysfile[256];
            int ret, fd;

            if (!read_until (&c, fp, buf, sizeof (buf), " :")) goto skiprest;

            for (pmp = parm_map; pmp->id != NULL; pmp++) {
                if (strcmp (buf, pmp->id)   == 0) break;
                if (strcmp (buf, pmp->full) == 0) break;
            }
            /* no match? next argument */
            if (pmp->id == NULL) goto skiprest;

            sprintf (sysfile, "%s/%s/bcache/%s", SYSPFX, argv[1], pmp->full);

            ret = read_until (&c, fp, buf, sizeof (buf), " ,");
            if (!ret && c != EOF && c > ' ') goto skiprest;

            fd = open(sysfile, O_WRONLY);
            if (fd < 0) error (0, errno, "Error opening %s", sysfile);

            dprintf (fd, "%s\n", buf);

            close (fd);

            if (!ret) break;
        }
    skiprest:
        while (c != EOF && c != ' ') c = fgetc (fp);
    }

return 0;
    if (dprintf(fd, "%s\n", argv[1]) < 0)
    {
        fprintf(stderr, "Error registering %s with bcache: %m\n", argv[1]);
        return 1;
    }

    return 0;
}

