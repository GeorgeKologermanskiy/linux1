#ifndef FILE_RW_C
#define FILE_FW_C

#include "structs.h"
#include "ret_values.h"
#include "rm.c"
#include "file.c"

int find_fd(FILE *fd, FSMetaData *data, OpenFileType type, int iNodePar, int iNode);

int open_file(FILE *fd, FSMetaData *data, char * filename, OpenFileType type, int iNode)
{
    //printf("open_file filename: %s %ld\n", filename, strlen(filename));

    int len = 0;
    while(filename[len] != '/' && filename[len] != 0)
    {
        ++len;
    }

    // read dir info of current iNode
    Info dirInfo;
    fseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode, SEEK_SET);
    fread(&dirInfo, sizeof(Info), 1, fd);
    
    // not found
    if (0 == dirInfo.countData)
    {
        return -2;
    }

    //printf("dirinfo: countData %d, next node %d\n", dirInfo.countData, dirInfo.iNodeNext);

    // read all items

    //printf("Start read items\n");
    Item* items = (Item*) malloc(sizeof(Item) * dirInfo.countData);
    fseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode + sizeof(Info), SEEK_SET);
    fread(items, sizeof(Item), dirInfo.countData, fd);
    //printf("Got Items\n");

    for (int pos = 0; pos < dirInfo.countData; ++pos)
    {
        Item* curr = items + pos;
        //printf("%d: %s\n", pos, curr->name);
        int curr_len = strlen(curr->name);
        int iNodeLoc = curr->iNodeLoc;
        if (curr_len != len)
        {
            continue;
        }
        if (strncmp(curr->name, filename, len) == 0)
        {
            if (filename[len] == '/')
            {
                // dir file problem
                if (IT_FILE == curr->type)
                {
                    free(items);
                    return -3;
                }
                //printf("Go to subdir\n");
                free(items);
                return open_file(fd, data, filename + len + 1, type, iNodeLoc);
            }
            else
            {
                // dir file problem
                if (IT_DIRECTORY == curr->type)
                {
                    free(items);
                    return -3;
                }
                //printf("Find dir, go build ls\n");
                int iNodeOld = curr->iNodeLoc;
                if (OFT_WRITE == type)
                {
                    curr->iNodeLoc = NewINode(fd, data, iNode, IT_FILE, 0);
                    fseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE + sizeof(Info), SEEK_SET);
                    fwrite(items, sizeof(Item), dirInfo.countData, fd); 
                }
                iNodeLoc = curr->iNodeLoc;
                free(items);
                int ret = find_fd(fd, data, type, iNode, iNodeLoc);
                if (OFT_WRITE == type)
                {
                    rm_solve_file(fd, data, iNodeOld);
                }
                return ret;
            }
        }
    }
    free(items);

    if (dirInfo.iNodeNext != 0)
    {
        return open_file(fd, data, filename, type, dirInfo.iNodeNext);
    }

    // not found
    return -2;
}

int find_fd(FILE *fd, FSMetaData *data, OpenFileType type, int iNodePar, int iNode)
{
    for (int pos = 0; pos < MAX_FD; ++pos)
    {
        if (data->fd[pos].iNode != 0)
        {
            continue;
        }
        data->fd[pos].openType = type;
        data->fd[pos].iNode = iNode;
        data->fd[pos].iNodePar = iNodePar;
        fseek(fd, 0, SEEK_SET);
        fwrite(data, sizeof(FSMetaData), 1, fd);
        return pos;
    }
    return -1;
}

int close_file(FILE *fd, FSMetaData *data, int pos)
{
    if (0 <= pos && pos < MAX_FD)
    {
        data->fd[pos].iNode = 0;
        fseek(fd, 0, SEEK_SET);
        fwrite(data, sizeof(FSMetaData), 1, fd);
        return 1;
    }
    return 0;
}

#endif