#ifndef DIR_C
#define DIR_C

#include <stdio.h>

#include "structs.h"
#include "ret_values.h"

void make_dir(FILE* fd, FSMetaData* data, char* dirname, int iNode, mkItemRet* ret)
{
    ret->isBadOperation = 0;
    ret->isExistDir = 0;
    ret->isExistFile = 0;

    //printf("curr dirname %s with iNode %d\n", dirname, iNode);

    // read dir info of current iNode
    Info dirInfo;
    fseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode, SEEK_SET);
    fread(&dirInfo, sizeof(Info), 1, fd);

    //printf("dirinfo: countData %d, next node %d\n", dirInfo.countData, dirInfo.iNodeNext);

    // read all items
    Item* items = NULL;
    if (dirInfo.countData > 0)
    {
        //printf("Start read items\n");
        items = (Item*) malloc(sizeof(Item) * dirInfo.countData);
        fseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode + sizeof(Info), SEEK_SET);
        fread(items, sizeof(Item), dirInfo.countData, fd);
        //printf("Got Items\n");
    }

    int dir_len = 0;
    while(dirname[dir_len] != '/' && dirname[dir_len] != 0)
    {
        ++dir_len;
    }

    //printf("dir len: %d\n", dir_len);

    // try fo find exist
    for (int pos = 0; pos < dirInfo.countData; ++pos)
    {
        Item* curr = items + pos;
        int len = strlen(curr->name);
        if (len != dir_len)
        {
            continue;
        }
        if (strncmp(curr->name, dirname, len) == 0)
        {
            if (IT_FILE == curr->type)
            {
                ret->isExistFile = 1;
                free(items);
                return;
            }
            if (dirname[len] == 0)
            {
                ret->isExistDir = 1;
                free(items);
                return;
            }
            if (dirname[len] == '/')
            {
                //printf("Go to subdir\n");
                make_dir(fd, data, dirname + len + 1, curr->iNodeLoc, ret);
                free(items);
                return;
            }
        }
    }
    free(items);

    // try to find in the next dir part
    if (dirInfo.iNodeNext != 0)
    {
        //printf("Go find in next node\n");
        make_dir(fd, data, dirname, dirInfo.iNodeNext, ret);
        return;
    }

    // try to add new dir
    if (sizeof(Info) + (dirInfo.countData + 1) * sizeof(Item) > CHUNK_SIZE)
    {
        //printf("new next iNode\n");
        dirInfo.iNodeNext = NewINode(fd, data, iNode, IT_DIRECTORY, 0);
        fseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode, SEEK_SET);
        fwrite(&dirInfo, sizeof(Info), 1, fd);
        make_dir(fd, data, dirname, dirInfo.iNodeNext, ret);
        return;
    }

    //printf("just next in list\n");

    // write new item
    Item new_item;
    new_item.iNodeLoc = NewINode(fd, data, iNode, IT_DIRECTORY, 0);
    new_item.type = IT_DIRECTORY;
    strncpy(new_item.name, dirname, dir_len);
    new_item.name[dir_len] = 0;
    fseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode + sizeof(Info) + dirInfo.countData * sizeof(Item), SEEK_SET);
    fwrite(&new_item, sizeof(Item), 1, fd);

    // update dir info
    ++dirInfo.countData;
    fseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode, SEEK_SET);
    fwrite(&dirInfo, sizeof(Info), 1, fd);

    if (dirname[dir_len] == '/')
    {
        make_dir(fd, data, dirname + dir_len + 1, new_item.iNodeLoc, ret);
    }
}

#endif