#ifndef LS_C
#define LS_C

#include "structs.h"
#include "ret_values.h"

int ls_solve(FILE *fd, FSMetaData* data, int iNode, lsRetItem *ret);

int ls(FILE *fd, FSMetaData* data, char *dirname, int iNode, lsRetItem *ret)
{
    if (NULL == dirname)
    {
        return ls_solve(fd, data, 0, ret);
    }

    int len = 0;
    while(dirname[len] != '/' && dirname[len] != 0)
    {
        ++len;
    }

    // read dir info of current iNode
    Info dirInfo;
    fseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode, SEEK_SET);
    fread(&dirInfo, sizeof(Info), 1, fd);

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
        if (strncmp(curr->name, dirname, len) == 0)
        {

            if (IT_FILE == curr->type)
            {
                free(items);
                return -1;
            }
            if (dirname[len] == '/')
            {
                //printf("Go to subdir\n");
                free(items);
                return ls(fd, data, dirname + len + 1, iNodeLoc, ret);
            }
            else
            {
                //printf("Find dir, go build ls\n");
                free(items);
                return ls_solve(fd, data, iNodeLoc, ret);
            }
        }
    }
    free(items);

    if (dirInfo.iNodeNext != 0)
    {
        return ls(fd, data, dirname, dirInfo.iNodeNext, ret);
    }

    return -2;
}

int ls_solve(FILE *fd, FSMetaData* data, int iNode, lsRetItem *ret)
{
    // read dir info of current iNode
    Info dirInfo;
    fseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode, SEEK_SET);
    fread(&dirInfo, sizeof(Info), 1, fd);

    //printf("dirinfo: countData %d, next node %d\n", dirInfo.countData, dirInfo.iNodeNext);

    // read all items

    //printf("Start read items\n");
    Item* items = (Item*) malloc(sizeof(Item) * dirInfo.countData);
    fseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode + sizeof(Info), SEEK_SET);
    fread(items, sizeof(Item), dirInfo.countData, fd);
    //printf("Got Items\n");
    
    int result = dirInfo.countData;

    lsRetItem *retNext = ret;
    if (NULL != ret)
    {
        retNext += dirInfo.countData;
        for (int pos = 0; pos < dirInfo.countData; ++pos)
        {
            memcpy(&ret[pos].item, items + pos, sizeof(Item));
        }
    }

    if (dirInfo.iNodeNext != 0)
    {
        result += ls_solve(fd, data, dirInfo.iNodeNext, retNext);
    }

    return result;
}

#endif