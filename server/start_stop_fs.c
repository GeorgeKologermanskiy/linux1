#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "structs.h"
#include "dir.c"
#include "config.c"

int start_fs(FSMetaData *data)
{
    int fd = open(driverFilename, O_CREAT | O_RDWR);
    if (-1 == fd)
    {
        return -1;
    }
    lseek(fd, 0, SEEK_SET);
    read(fd, data, sizeof(FSMetaData));
    
    if (0 == data->lastBlockNum)
    {
        NewINode(fd, data, -1, IT_DIRECTORY, 0);
    }

    return fd;
}

void stop_fs(int fd, FSMetaData *data)
{
    if (-1 != fd)
    {
        lseek(fd, 0, SEEK_SET);
        write(fd, data, sizeof(FSMetaData));
        close(fd);
    }
}