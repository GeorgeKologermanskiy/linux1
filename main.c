#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "start_stop_fs.c"
#include "dir.c"
#include "file.c"
#include "ls.c"
#include "rm.c"
#include "file_open_close.c"
#include "file_read_write.c"

int main(int argc, char ** argv)
{
    FSMetaData data;
    FILE* fd = start_fs(argc, argv, &data);
    if (NULL == fd)
    {
        printf("Unable to start FS\n");
        return 0;
    }

    char *line = NULL;
    size_t line_len = 0;
    for(;;)
    {
        printf("\n>");
        int res = getline(&line, &line_len, stdin);
        if (res <= 0)
        {
            printf("Unable to read line\n");
            continue;
        }
        line[res - 1] = 0;

        if (strncmp(line, "ls", 2) == 0)
        {
            char *dirname = NULL;
            if (res > 3)
            {
                dirname = line + 3;
            }
            int count = ls(fd, &data, dirname, 0, NULL);
            if (-1 == count)
            {
                printf("There is a file in directory path\n");
                continue;
            }
            if (-2 == count)
            {
                printf("Directory not found\n");
                continue;
            }
            if (0 == count)
            {
                continue;
            }
            lsRetItem *ret = (lsRetItem*)malloc(count * sizeof(lsRetItem));
            ls(fd, &data, dirname, 0, ret);
            for (int pos = 0; pos < count; ++pos)
            {
                lsRetItem *curr = ret + pos;
                if (IT_FILE == curr->item.type)
                {
                    printf("FILE:\t\t iNode - %d\t name - %s\n", curr->item.iNodeLoc, curr->item.name);
                }
                if (IT_DIRECTORY == curr->item.type)
                {
                    printf("DIRECTORY:\t iNode - %d\t name - %s\n", curr->item.iNodeLoc, curr->item.name);
                }
            }
            free(ret);
            continue;
        }

        if (strncmp(line, "rm", 2) == 0)
        {
            rmRet ret;
            rm(fd, &data, line + 3, 0, &ret);
            if (ret.isFileDirProblem)
            {
                printf("There is file in remove path, but have to be directory\n");
            }
            else if(!ret.isFoundItem)
            {
                printf("Item not found\n");
            }
            else if(ret.isFile)
            {
                printf("File %s deleted\n", line + 3);
            }
            else
            {
                printf("Directory %s deleted\n", line + 3);
            }
            continue;
        }

        if (strncmp(line, "exit", 4) == 0)
        {
            break;
        }

        if (strncmp(line, "mkdir", 5) == 0)
        {
            mkItemRet ret;
            make_dir(fd, &data, line + 6, 0, &ret);
            if (ret.isBadOperation)
            {
                printf("Problem with operation\n");
            }
            else if (ret.isExistDir)
            {
                printf("This directory is exist\n");
            }
            else if (ret.isExistFile)
            {
                printf("There is file with this name\n");
            }
            else
            {
                printf("Directory %s is created\n", line + 6);
            }
            continue;
        }

        if (strncmp(line, "mkfile", 6) == 0)
        {
            mkItemRet ret;
            make_file(fd, &data, line + 7, 0, &ret);
            if (ret.isBadOperation)
            {
                printf("Problem with operation\n");
            }
            else if (ret.isExistDir)
            {
                printf("There is directory with the same name\n");
            }
            else if (ret.isExistFile)
            {
                printf("There is file with this name or file in name which have to be directory\n");
            }
            else
            {
                printf("File %s is created\n", line + 7);
            }
            continue;
        }

        if (strncmp(line, "open_file", 9) == 0)
        {
            int typePos = 10;
            while(line[typePos] != ' ' && line[typePos] != 0)
            {
                ++typePos;
            }

            if (0 == line[typePos])
            {
                printf("Invalid syntax for open_file\n");
                continue;
            }
            line[typePos] = 0;
            ++typePos;

            OpenFileType type = OFT_READ;
            if (line[typePos] == 'r')
            {
                type = OFT_READ;
            }
            else if(line[typePos] == 'w')
            {
                type = OFT_WRITE;
            }
            else
            {
                printf("Invalid open file type\n");
            }

            int ret = open_file(fd, &data, line + 10, type, 0);
            if (-3 == ret)
            {
                printf("There is a file in path, which have to be directory\n");
            }
            else if (-2 == ret)
            {
                printf("File not found for open\n");
            }
            else if (-1 == ret)
            {
                printf("All fd are used. Close one and retry\n");
            }
            else if (OFT_READ == type)
            {
                printf("FD for read: %d\n", ret);
            }else
            {
                printf("FD for write: %d\n", ret);
            }
            continue;
        }

        if (strncmp(line, "read_file", 9) == 0)
        {
            int pos;
            int from;
            int count;
            sscanf(line + 10, "%d %d %d", &pos, &from, &count);
            char *buff = (char*)malloc(sizeof(char) * count);
            int ret = read_file(fd, &data, pos, buff, from, count);
            if (-2 == ret)
            {
                printf("Incorrect FD\n");
            }
            if (-1 == ret)
            {
                printf("FD %d is opened for write\n", pos);
            }
            if (0 == ret)
            {
                printf("Result: %s\n", buff);
            }
            free(buff);
            continue;
        }

        if (strncmp(line, "close_file", 10) == 0)
        {
            int pos;
            sscanf(line + 11, "%d", &pos);
            int ret = close_file(fd, &data, pos);
            if (0 == ret)
            {
                printf("Incorrect FD\n");
            }
            else if (1 == ret)
            {
                printf("FD %d is closed\n", pos);
            }
            else
            {
                printf("Unknown behavior\n");
            }
            continue;
        }

        if (strncmp(line, "write_file", 10) == 0)
        {
            char *end;
            int pos = strtol(line + 11, &end, 10);
            int buff_start = 11;
            while(line[buff_start] != ' ' && line[buff_start] != 0)
            {
                ++buff_start;
            }
            ++buff_start;
            int ret = write_file(fd, &data, pos , line + buff_start);
            if (-2 == ret)
            {
                printf("Invalid FD: %d\n", pos);
                continue;
            }
            if (-1 == ret)
            {
                printf("Descriptor %d is opening fir read\n", pos);
                continue;
            }
            if (0 == ret)
            {
                printf("Complete\n");
                continue;
            }
            printf("Undefined behavior\n");
            continue;
        }


        printf("Invalid syntax\n");
    }

    stop_fs(fd, &data);
    return 0;
}