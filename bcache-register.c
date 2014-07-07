/*
 * Author: Simon Gomizelj <simongmzlj@gmail.com>
 *
 * GPLv2
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
    int fd, i, rc = 0;

    fd = open("/sys/fs/bcache/register_quiet", O_WRONLY);
    if (fd < 0)
        return 1;

    for (i = 1; i < argc; ++i)
        rc |= (dprintf(fd, "%s\n", argv[i]) < 0) ? 1 : 0;

    close(fd);
    return rc;
}

// vim: et:sts=4:sw=4:cino=(0
