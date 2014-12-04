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
    int fd;

    if (argc != 2)
    {
        fprintf(stderr, "bcache-register takes exactly one argument\n");
        return 1;
    }

    fd = open("/sys/fs/bcache/register", O_WRONLY);
    if (fd < 0)
    {
        perror("Error opening /sys/fs/bcache/register");
        fprintf(stderr, "The bcache kernel module must be loaded\n");
        return 1;
    }

    if (dprintf(fd, "%s\n", argv[1]) < 0)
    {
        fprintf(stderr, "Error registering %s with bcache: %m\n", argv[1]);
        return 1;
    }

    return 0;
}

