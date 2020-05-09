#ifndef FILE_READ_WRITE_C
#define FILE_READ_WRITE_C

#include "structs.h"
#include "ret_values.h"
#include <math.h>

int write_file_ret_iNode(int fd, FSMetaData *data, int iNode, char *buff, int len, int *written, int canIncreaseDepth);

int write_file(int fd, FSMetaData *data, int file_fd, char *buff, int len)
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

    int written;
    int iNodeFrom = data->fd[file_fd].iNode;
    data->fd[file_fd].iNode = write_file_ret_iNode(fd, data, iNodeFrom, buff, len, &written, 1);
    if (iNodeFrom != data->fd[file_fd].iNode)
    {
        //printf("%d is not %d\n", iNodeFrom, data->fd[file_fd].iNode);
        change(fd, data, data->fd[file_fd].iNodePar, iNodeFrom, data->fd[file_fd].iNode);
    }
    return 0;
}

int write_file_ret_iNode(int fd, FSMetaData *data, int iNode, char *buff, int len, int *written, int canIncreaseDepth)
{
    //printf("\nTry to write in iNode %d %s with len %d canInc - %d\n", iNode, buff, len, canIncreaseDepth);
    if (len == 0)
    {
        *written = 0;
        return iNode;
    }

    // read file info
    Info fileInfo;
    lseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE, SEEK_SET);
    read(fd, &fileInfo, sizeof(Info));

    //printf("fileInfo: countData - %d, depth - %d, free space - %d\n", fileInfo.countData, fileInfo.depth, fileInfo.freeSpace);

    if (fileInfo.freeSpace >= len || 0 == canIncreaseDepth)
    {
        //printf("Enough Space or cant increase\n");
        if (0 == fileInfo.freeSpace)
        {
            //printf("Dont write something\n");
            *written = 0;
            return iNode;
        }
        if (0 == fileInfo.depth)
        {
            int w = (fileInfo.freeSpace < len ? fileInfo.freeSpace : len);
            //printf("0 depth, try to write %d\n", w);
            *written += w;
            lseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE + sizeof(Info) + fileInfo.countData, SEEK_SET);
            write(fd, buff, sizeof(char) * w);
            
            //printf("Write in iNode %d %d bytes\n", iNode, w);

            fileInfo.freeSpace -= w;
            fileInfo.countData += w;
            lseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE, SEEK_SET);
            write(fd, &fileInfo, sizeof(Info));
            return iNode;
        }

        // read items
        FileLink* items = (FileLink*)malloc(sizeof(FileLink) * MAX_LINK_COUNT);
        lseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE + sizeof(Info), SEEK_SET);
        read(fd, items, sizeof(FileLink) * fileInfo.countData);

        for (int pos = 0; pos < MAX_LINK_COUNT; ++pos)
        {
            if (0 == len)
            {
                fileInfo.countData = pos;
                break;
            }
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
        lseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE, SEEK_SET);
        write(fd, &fileInfo, sizeof(Info));

        // update items
        lseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE + sizeof(Info), SEEK_SET);
        write(fd, items, sizeof(FileLink) * fileInfo.countData);
        free(items);
        return iNode;
    }

    int w = 0;
    write_file_ret_iNode(fd, data, iNode, buff, fileInfo.freeSpace, &w, 0);
    *written += w;
    buff += w;
    len -= w;
    // update file info
    lseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE, SEEK_SET);
    read(fd, &fileInfo, sizeof(Info));
    
    int iNodeRet = NewINode(fd, data, fileInfo.iNodePrev, IT_FILE, fileInfo.depth + 1);

    //printf("iNode for increase: %d\n", iNodeRet);

    // add link iNodeRet to iNode
    FileLink link;
    link.iNode = iNode;
    lseek(fd, sizeof(FSMetaData) + iNodeRet * CHUNK_SIZE + sizeof(Info), SEEK_SET);
    write(fd, &link, sizeof(FileLink));

    // increase count data in iNodeRet
    // and decrease freeSpace
    Info fileInfoRet;
    lseek(fd, sizeof(FSMetaData) + iNodeRet * CHUNK_SIZE, SEEK_SET);
    read(fd, &fileInfoRet, sizeof(Info));
    fileInfoRet.countData = 1;
    fileInfoRet.freeSpace -= MAX_CHUNK_SPACE * power(MAX_LINK_COUNT, fileInfo.depth);
    lseek(fd, sizeof(FSMetaData) + iNodeRet * CHUNK_SIZE, SEEK_SET);
    write(fd, &fileInfoRet, sizeof(Info));

    // update parent link in file info
    fileInfo.iNodePrev = iNodeRet;
    lseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE, SEEK_SET);
    write(fd, &fileInfo, sizeof(Info));

    return write_file_ret_iNode(fd, data, iNodeRet, buff, len, written, 1);
}

void read_file_dfs(int fd, FSMetaData *data, int iNode, char **buff, int *from, int *len);

int read_file(int fd, FSMetaData *data, int file_fd, char *buff, int from, int len)
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

void read_file_dfs(int fd, FSMetaData *data, int iNode, char **buff, int *from, int *len)
{
    //printf("\nRead from iNode %d\n", iNode);
    if (*len == 0)
    {
        return;
    }
    // read file info
    Info fileInfo;
    lseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode, SEEK_SET);
    read(fd, &fileInfo, sizeof(Info));

    //printf("fileInfo: countData - %d, depth - %d, free space - %d\n", fileInfo.countData, fileInfo.depth, fileInfo.freeSpace);

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
        lseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode + sizeof(Info) + *from, SEEK_SET);
        read(fd, *buff, sizeof(char) * size);
        *from = 0;
        *buff += size;
        *len -= size;
        return;
    }

    //printf("Depth more than 0, go recursive\n");

    FileLink *links = (FileLink*)malloc(sizeof(FileLink) * fileInfo.countData);
    lseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode + sizeof(Info), SEEK_SET);
    read(fd, links, sizeof(FileLink) * fileInfo.countData);
    for (int pos = 0; pos < fileInfo.countData; ++pos)
    {
        read_file_dfs(fd, data, links[pos].iNode, buff, from, len);
    }
    free(links);
}

#endif