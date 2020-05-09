#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdio.h>

typedef enum OpenFileType
{
    OFT_READ,
    OFT_WRITE
} OpenFileType;

typedef struct FDStruct
{
    OpenFileType openType;
    int iNodePar;
    int iNode;
} FDStruct;

#define MAX_FD 16

typedef struct FSMetaData
{
    FDStruct fd[MAX_FD];
    int lastBlockNum;
} FSMetaData;

typedef enum ItemType
{
    IT_DIRECTORY,
    IT_FILE,
    IT_OTHER
} ItemType;

typedef struct Info
{
    int iNodeCurr;
    ItemType type;
    int iNodePrev;
    int iNodeNext;
    int countData;
    // only for files
    int depth;
    int freeSpace;
} Info;

#define MAX_PATH 20

typedef struct Item
{
    int iNodeLoc;
    char name[MAX_PATH];
    ItemType type;
} Item;

typedef struct FileLink
{
    int iNode;
} FileLink;

#define CHUNK_SIZE 128
// (sizeof(Info) + 2 * sizeof(Item));

#define MAX_CHUNK_SPACE (CHUNK_SIZE - sizeof(Info))

#define MAX_LINK_COUNT  (MAX_CHUNK_SPACE / sizeof(FileLink))

int power(int a, int n)
{
    if (n == 0)
    {
        return 1;
    }
    if (n == 1)
    {
        return a;
    }
    if (n & 1)
    {
        return a * power(a, n - 1);
    }
    int b = power(a, n >> 1);
    return b * b;
}

int NewINode(int fd, FSMetaData *data, int parent, ItemType type, int depth)
{
    //printf("Create iNode %d, type - %d\n", data->lastBlockNum, type);
    lseek(fd, sizeof(FSMetaData) + data->lastBlockNum * CHUNK_SIZE, SEEK_SET);
    Info info;
    info.iNodeCurr = -1;
    info.iNodePrev = parent;
    info.countData = 0;
    info.iNodeNext = 0;
    info.type = type;
    info.depth = depth;
    info.freeSpace = MAX_CHUNK_SPACE * power(MAX_LINK_COUNT, depth);
    write(fd, &info, sizeof(Info));

    // alloc
    ++data->lastBlockNum;
    lseek(fd, sizeof(FSMetaData) + data->lastBlockNum * CHUNK_SIZE, SEEK_SET);
    write(fd, "end", sizeof(char) * 3);

    lseek(fd, 0, SEEK_SET);
    write(fd, data, sizeof(FSMetaData));
    return data->lastBlockNum - 1;
}

void change(int fd, FSMetaData* data, int iNode, int iNodeFrom, int iNodeTo)
{
    //printf("Change in %d iNode from %d to %d\n", iNode, iNodeFrom, iNodeTo);

    Info info;
    lseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode, SEEK_SET);
    read(fd, &info, sizeof(Info));

    for (int pos = 0; pos < MAX_FD; ++pos)
    {
        if (iNodeFrom == data->fd[pos].iNode)
        {
            data->fd[pos].iNode = iNodeTo;
        }
        if (iNodeFrom == data->fd[pos].iNodePar)
        {
            data->fd[pos].iNodePar = iNodeTo;
        }
    }

    lseek(fd, 0, SEEK_SET);
    write(fd, data, sizeof(FSMetaData));

    if (IT_DIRECTORY == info.type)
    {
        //printf("%d is Directory\n", iNode);

        // read items
        Item* items = (Item*) malloc(sizeof(Item) * info.countData);
        lseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode + sizeof(Info), SEEK_SET);
        read(fd, items, sizeof(Item) * info.countData);

        // change iNodes
        for (int pos = 0; pos < info.countData; ++pos)
        {
            if (iNodeFrom == items[pos].iNodeLoc)
            {
                items[pos].iNodeLoc = iNodeTo;
            }
        }

        // write items
        lseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode + sizeof(Info), SEEK_SET);
        write(fd, items, sizeof(Item) * info.countData);

        free(items);
        return;
    }

    //printf("%d is File with %d depth\n", iNode, info.depth);

    if (0 == info.depth)
    {
        return;
    }

    // read links
    FileLink *links = (FileLink*)malloc(sizeof(FileLink) * info.countData);
    lseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode + sizeof(Info), SEEK_SET);
    read(fd, links, sizeof(FileLink) * info.countData);

    // change links
    for (int pos = 0; pos < info.countData; ++pos)
    {
        if (iNodeFrom == links[pos].iNode)
        {
            links[pos].iNode = iNodeTo;
        }
    }

    lseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode + sizeof(Info), SEEK_SET);
    write(fd, links, sizeof(FileLink) * info.countData);

    free(links);
}

void DelINode(int fd, FSMetaData *data, int iNode)
{
    //printf("Delete %d iNode\n", iNode);

    --data->lastBlockNum;
    lseek(fd, 0, SEEK_SET);
    write(fd, data, sizeof(FSMetaData));

    if (data->lastBlockNum == iNode)
    {
        return;
    }
    // get last block
    char *mem = (char*)malloc(sizeof(char) * CHUNK_SIZE);
    lseek(fd, sizeof(FSMetaData) + data->lastBlockNum * CHUNK_SIZE, SEEK_SET);
    read(fd, mem, CHUNK_SIZE * sizeof(char));

    // write him to iNode
    lseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE, SEEK_SET);
    write(fd, mem, CHUNK_SIZE * sizeof(char));

    // free memory
    free(mem);

    // change 
    Info dirInfo;
    lseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode, SEEK_SET);
    read(fd, &dirInfo, sizeof(Info));
    
    change(fd, data, dirInfo.iNodePrev, data->lastBlockNum, iNode);
}

#endif
