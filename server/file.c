#ifndef FILE_C
#define FILE_C

#include "structs.h"
#include "dir.c"

void make_file(int fd, FSMetaData* data, char* filename, int iNode, mkItemRet *ret)
{
    ret->isBadOperation = 0;
    ret->isExistDir = 0;
    ret->isExistFile = 0;

    //printf("curr filename %s with iNode %d\n", filename, iNode);

    int len = 0;
    while(filename[len] != '/' && filename[len] != 0)
    {
        ++len;
    }

    if (filename[len] == '/')
    {
        filename[len] = 0;
        make_dir(fd, data, filename, iNode, ret);
        if (ret->isExistFile || ret->isBadOperation)
        {
            return;
        }
        //printf("is exist dir: %d\n", ret->isExistDir);
        ret->isExistDir = 0;
        filename[len] = '/';
    }

    // read dir info of current iNode
    Info dirInfo;
    lseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode, SEEK_SET);
    read(fd, &dirInfo, sizeof(Info));

    //printf("dirinfo: countData %d, next node %d\n", dirInfo.countData, dirInfo.iNodeNext);

    // read all items

    //printf("Start read items\n");
    Item* items = (Item*) malloc(sizeof(Item) * dirInfo.countData);
    lseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode + sizeof(Info), SEEK_SET);
    read(fd, items, sizeof(Item) * dirInfo.countData);
    //printf("Got Items\n");
    
    // try fo find dir
    for (int pos = 0; pos < dirInfo.countData; ++pos)
    {
        Item* curr = items + pos;
        //printf("%d: %s\n", pos, curr->name);
        int curr_len = strlen(curr->name);
        if (curr_len != len)
        {
            continue;
        }
        if (strncmp(curr->name, filename, len) == 0)
        {
            if (IT_DIRECTORY == curr->type && filename[len] == 0)
            {
                ret->isExistDir = 1;
                free(items);
                return;
            }
            if (IT_FILE == curr->type)
            {
                ret->isExistFile = 1;
                free(items);
                return;
            }
            //printf("Go to subdir\n");
            make_file(fd, data, filename + len + 1, curr->iNodeLoc, ret);
            free(items);
            return;
        }
    }
    free(items);
    if (dirInfo.iNodeNext != 0)
    {
        //printf("Find in next iNode\n");
        make_file(fd, data, filename, dirInfo.iNodeNext, ret);
        return;
    }

    // it is a file

    // if cant add in current iNode
    if (sizeof(Info) + (dirInfo.countData + 1) * sizeof(Item) > CHUNK_SIZE)
    {
        //printf("new next iNode\n");
        dirInfo.iNodeNext = NewINode(fd, data, iNode, IT_DIRECTORY, 0);
        lseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode, SEEK_SET);
        write(fd, &dirInfo, sizeof(Info));
        make_file(fd, data, filename, dirInfo.iNodeNext, ret);
        return;
    }

    // add new file in current iNode

    //printf("Add as next in list\n");

    Item new_item;
    new_item.type = IT_FILE;
    new_item.iNodeLoc = NewINode(fd, data, iNode, IT_FILE, 0);
    strncpy(new_item.name, filename, len);
    new_item.name[len] = 0;
    lseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode + sizeof(Info) + dirInfo.countData * sizeof(Item), SEEK_SET);
    write(fd, &new_item, sizeof(Item));

    // update dir info
    ++dirInfo.countData;
    lseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode, SEEK_SET);
    write(fd, &dirInfo, sizeof(Info));
}

#endif