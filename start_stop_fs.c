#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "structs.h"
#include "dir.c"

FILE* start_fs(int argc, char** argv, FSMetaData *data)
{
    if (1 == argc)
    {
        printf("Can`t start without args\n");
        return NULL;
    }
    char *filename = argv[1];
    int res = access(filename, F_OK);
    FILE* fd;
    if (-1 != res)
    {
        fd = fopen(filename, "r+");
        if (NULL == fd)
        {
            return NULL;
        }
        fseek(fd, 0, SEEK_SET);
        fread(data, sizeof(FSMetaData), 1, fd);
    }
    else
    {
        fd = fopen(filename, "w+");
        if (NULL == fd)
        {
            return NULL;
        }

        // fill metadata
        data->lastBlockNum = 0;
        /*for (int i = 0; i < MAX_FD; ++i)
        {
            data->fd[i].iNodeNum = 0;
            data->fd[i].iNodeOffset = 0;
        }*/
        fwrite(data, sizeof(FSMetaData), 1, fd);

        NewINode(fd, data, -1, IT_DIRECTORY, 0);
    }
    return fd;
}

void stop_fs(FILE* fd, FSMetaData *data)
{
    if (NULL != fd)
    {
        fseek(fd, 0, SEEK_SET);
        fwrite(data, sizeof(FSMetaData), 1, fd);
        fclose(fd);
    }
}