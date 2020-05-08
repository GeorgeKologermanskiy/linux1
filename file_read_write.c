#ifndef FILE_READ_WRITE_C
#define FILE_READ_WRITE_C

#include "structs.h"
#include "ret_values.h"
#include <math.h>

int write_file_ret_iNode(FILE *fd, FSMetaData *data, int iNode, char *buff, int len, int *written, int canIncreaseDepth);

int write_file(FILE *fd, FSMetaData *data, int file_fd, char *buff)
{
    // incorrect file_fd
    if (file_fd < 0 || MAX_FD <= file_fd)
    {
        return -2;
    }

    if (0 == data->fd[file_fd].iNode)
    {
        return -2;
    }

    // incorrect operation type
    if (OFT_READ == data->fd[file_fd].openType)
    {
        return -1;
    }

    int iNodeHead = data->fd[file_fd].iNode;
    int len = strlen(buff);
    int written;
    int iNodeFrom = data->fd[file_fd].iNode;
    data->fd[file_fd].iNode = write_file_ret_iNode(fd, data, iNodeHead, buff, len, &written, 1);
    change(fd, data, data->fd[file_fd].iNodePar, iNodeFrom, data->fd[file_fd].iNode);
    fseek(fd, 0, SEEK_SET);
    fwrite(data, sizeof(FSMetaData), 1, fd);
    return 0;
}

int write_file_ret_iNode(FILE *fd, FSMetaData *data, int iNode, char *buff, int len, int *written, int canIncreaseDepth)
{
    //printf("Try to write in iNode %d with len %d canInc - %d\n", iNode, len, canIncreaseDepth);
    if (len == 0)
    {
        written = 0;
        return iNode;
    }

    // read file info
    Info fileInfo;
    fseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE, SEEK_SET);
    fread(&fileInfo, sizeof(Info), 1, fd);

    //printf("fileInfo: countData - %d, depth - %d, free space - %d\n", fileInfo.countData, fileInfo.depth, fileInfo.freeSpace);

    if (fileInfo.freeSpace >= len || 0 == canIncreaseDepth)
    {
        //printf("Enough Space or cant increase\n");
        if (0 == fileInfo.freeSpace)
        {
            return iNode;
        }
        if (0 == fileInfo.depth)
        {
            int w = (fileInfo.freeSpace < len ? fileInfo.freeSpace : len);
            *written += w;
            fseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE + sizeof(Info) + fileInfo.countData, SEEK_SET);
            fwrite(buff, sizeof(char), w, fd);
            fileInfo.freeSpace -= w;
            fileInfo.countData += w;
            fseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE, SEEK_SET);
            fwrite(&fileInfo, sizeof(Info), 1, fd);
            return iNode;
        }
        // read items
        FileLink* items = (FileLink*)malloc(sizeof(FileLink) * MAX_LINK_COUNT);
        fseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE + sizeof(Info), SEEK_SET);
        fread(items, sizeof(FileLink), fileInfo.countData, fd);

        for (int pos = 0; pos < MAX_LINK_COUNT; ++pos)
        {
            if (pos >= fileInfo.countData)
            {
                items[pos].iNode = NewINode(fd, data, iNode, IT_FILE, fileInfo.depth - 1);
            }
            int w = 0;
            write_file_ret_iNode(fd, data, items[pos].iNode, buff, len, &w, 0);
            *written += w;
            buff += w;
            len -= w;
            fileInfo.freeSpace -= w;
        }
        // update file info
        fileInfo.countData = MAX_LINK_COUNT;
        fseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE, SEEK_SET);
        fwrite(&fileInfo, sizeof(Info), 1, fd);

        // update items
        fseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE + sizeof(Info), SEEK_SET);
        fwrite(items, sizeof(FileLink), fileInfo.countData, fd);
        free(items);
        return iNode;
    }

    int w = 0;
    write_file_ret_iNode(fd, data, iNode, buff, fileInfo.freeSpace, &w, 0);
    *written += w;
    buff += w;
    len -= w;
    // update file info
    fseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE, SEEK_SET);
    fread(&fileInfo, sizeof(Info), 1, fd);
    
    int iNodeRet = NewINode(fd, data, fileInfo.iNodePrev, IT_FILE, fileInfo.depth + 1);

    //printf("iNode for increase: %d\n", iNodeRet);

    // add link iNodeRet to iNode
    FileLink link;
    link.iNode = iNode;
    fseek(fd, sizeof(FSMetaData) + iNodeRet * CHUNK_SIZE + sizeof(Info), SEEK_SET);
    fwrite(&link, sizeof(FileLink), 1, fd);

    // increase count data in iNodeRet
    // and decrease freeSpace
    Info fileInfoRet;
    fseek(fd, sizeof(FSMetaData) + iNodeRet * CHUNK_SIZE, SEEK_SET);
    fread(&fileInfoRet, sizeof(Info), 1, fd);
    fileInfoRet.countData = 1;
    fileInfoRet.freeSpace -= MAX_CHUNK_SPACE * power(MAX_LINK_COUNT, fileInfo.depth);
    fseek(fd, sizeof(FSMetaData) + iNodeRet * CHUNK_SIZE, SEEK_SET);
    fwrite(&fileInfoRet, sizeof(Info), 1, fd);

    // update parent link in file info
    fileInfo.iNodePrev = iNodeRet;
    fseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE, SEEK_SET);
    fwrite(&fileInfo, sizeof(Info), 1, fd);

    return write_file_ret_iNode(fd, data, iNodeRet, buff, len, written, 1);
}

void read_file_dfs(FILE *fd, FSMetaData *data, int iNode, char **buff, int *from, int *len);

int read_file(FILE *fd, FSMetaData *data, int file_fd, char *buff, int from, int len)
{
    // incorrect file_fd
    if (file_fd < 0 || MAX_FD <= file_fd)
    {
        return -2;
    }

    if (0 == data->fd[file_fd].iNode)
    {
        return -2;
    }

    // incorrect operation type
    if (OFT_WRITE == data->fd[file_fd].openType)
    {
        return -1;
    }

    read_file_dfs(fd, data, data->fd[file_fd].iNode, &buff, &from, &len);
    return 0;
}

void read_file_dfs(FILE *fd, FSMetaData *data, int iNode, char **buff, int *from, int *len)
{
    //printf("Read from iNode %d\n", iNode);
    if (*len == 0)
    {
        return;
    }
    // read file info
    Info fileInfo;
    fseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode, SEEK_SET);
    fread(&fileInfo, sizeof(Info), 1, fd);

    int hasInfo = MAX_CHUNK_SPACE * power(MAX_LINK_COUNT, fileInfo.depth) - fileInfo.freeSpace;

    if (hasInfo <= *from)
    {
        *from -= hasInfo;
        return;
    }

    if (0 == fileInfo.depth)
    {
        int size = (*len < (int)fileInfo.countData - *from ? *len : (int)fileInfo.countData - *from);
        //printf("Depth 0, go read %d\n", size);
        fseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode + sizeof(Info) + *from, SEEK_SET);
        fread(*buff, sizeof(char), size, fd);
        *from = 0;
        *buff += size;
        *len -= size;
        return;
    }

    //printf("Depth more than 0, go recursive\n");

    FileLink *links = (FileLink*)malloc(sizeof(FileLink) * fileInfo.countData);
    fseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode + sizeof(Info), SEEK_SET);
    fread(links, sizeof(FileLink), fileInfo.countData, fd);
    for (int pos = 0; pos < fileInfo.countData; ++pos)
    {
        read_file_dfs(fd, data, links[pos].iNode, buff, from, len);
    }
    free(links);
}

#endif