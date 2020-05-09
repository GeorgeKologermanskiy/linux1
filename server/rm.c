#ifndef RM_C
#define RM_C

#include "structs.h"
#include "ret_values.h"

void rm_solve_file(FILE *fd, FSMetaData *data, int iNode);

void rm_solve_dir(FILE *fd, FSMetaData *data, int iNode);

int find_last_iNode(FILE *fd, FSMetaData *data, int iNode);

void rm_from_list(FILE *fd, FSMetaData *data, int iNode, int pos);

void rm(FILE *fd, FSMetaData *data, char* name, int iNode, rmRet* ret)
{
    ret->isFile = 0;
    ret->isFileDirProblem = 0;
    ret->isFoundItem = 1;

    int len = 0;
    while(name[len] != '/' && name[len] != 0)
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
        if (curr_len != len)
        {
            continue;
        }
        if (strncmp(curr->name, name, len) == 0)
        {
            if (name[len] == '/')
            {
                if (IT_FILE == curr->type)
                {
                    free(items);
                    ret->isFileDirProblem = 1;
                    return;
                }
                //printf("Go to subdir\n");
                rm(fd, data, name + len + 1, curr->iNodeLoc, ret);
                free(items);
                return;
            }
            else
            {
                //printf("Found item, go remove\n");
                ret->isFoundItem = 1;
                if (IT_FILE == curr->type)
                {
                    ret->isFile = 1;
                    rm_solve_file(fd, data, curr->iNodeLoc);
                }
                else
                {
                    ret->isFile = 0;
                    rm_solve_dir(fd, data, curr->iNodeLoc);
                }
                
                rm_from_list(fd, data, iNode, pos);
                free(items);
                return;                
            }
        }
    }
    free(items);

    if (dirInfo.iNodeNext != 0)
    {
        rm(fd, data, name, dirInfo.iNodeNext, ret);
    }
    ret->isFoundItem = 0;
    return;
}

int find_last_iNode(FILE *fd, FSMetaData *data, int iNode)
{
    // read dir info of current iNode
    Info dirInfo;
    fseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode, SEEK_SET);
    fread(&dirInfo, sizeof(Info), 1, fd);
    
    if (dirInfo.iNodeNext == 0)
    {
        return iNode;
    }

    return find_last_iNode(fd, data, dirInfo.iNodeNext);
}

void rm_from_list(FILE *fd, FSMetaData *data, int iNode, int pos)
{
    // dirInfo iNode
    Info dirInfo;
    fseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode, SEEK_SET);
    fread(&dirInfo, sizeof(Info), 1, fd);

    if (dirInfo.iNodeNext == 0)
    {
        --dirInfo.countData;
        fseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode, SEEK_SET);
        fwrite(&dirInfo, sizeof(Info), 1, fd);
        if (pos == dirInfo.countData)
        {
            return;
        }
        Item* mem = (Item*) malloc(sizeof(Item));
        fseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE + sizeof(Info) + dirInfo.countData * sizeof(Item), SEEK_SET);
        fread(mem, sizeof(Item), 1, fd);
        fseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE + sizeof(Info) + pos * sizeof(Item), SEEK_SET);
        fwrite(mem, sizeof(Item), 1, fd);
        free(mem);
        return;
    }
    int iNodeLast = find_last_iNode(fd, data, dirInfo.iNodeNext);
    // change dirInfoLast
    Info dirInfoLast;
    fseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNodeLast, SEEK_SET);
    fread(&dirInfoLast, sizeof(Info), 1, fd);
    --dirInfoLast.countData;
    fseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNodeLast, SEEK_SET);
    fwrite(&dirInfoLast, sizeof(Info), 1, fd);

    // read last item
    Item* mem = (Item*) malloc(sizeof(Item));
    fseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNodeLast + sizeof(Info) + dirInfoLast.countData * sizeof(Item), SEEK_SET);
    fread(mem, sizeof(Item), 1, fd);


    // write him into 
    fseek(fd, sizeof(FSMetaData) + CHUNK_SIZE * iNode + sizeof(Info) + pos * sizeof(Item), SEEK_SET);
    fwrite(mem, sizeof(Item), 1, fd);

    free(mem);
}

void rm_solve_file(FILE *fd, FSMetaData *data, int iNode)
{
    Info fileInfo;
    fseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE, SEEK_SET);
    fread(&fileInfo, sizeof(Info), 1, fd);

    if (0 == fileInfo.depth)
    {
        DelINode(fd, data, iNode);
        return;
    }

    FileLink* links = (FileLink*)malloc(sizeof(FileLink) * fileInfo.countData);
    fseek(fd, sizeof(FSMetaData) + iNode * CHUNK_SIZE + sizeof(Info), SEEK_SET);
    fread(links, sizeof(FileLink), fileInfo.countData, fd);

    for (int pos = 0; pos < fileInfo.countData; ++pos)
    {
        rm_solve_file(fd, data, links[pos].iNode);
    }

    free(links);

    DelINode(fd, data, iNode);
    return;
}

void rm_solve_dir(FILE *fd, FSMetaData *data, int iNode)
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
    
    // try fo find dir
    for (int pos = 0; pos < dirInfo.countData; ++pos)
    {
        Item* curr = items + pos;
        //printf("%d: %s\n", pos, curr->name);
        if (IT_DIRECTORY == curr->type)
        {
            rm_solve_dir(fd, data, curr->iNodeLoc);
        }
        else
        {
            rm_solve_file(fd, data, curr->iNodeLoc);
        }
    }
    free(items);

    if (dirInfo.iNodeNext != 0)
    {
        rm_solve_dir(fd, data, dirInfo.iNodeNext);
    }

    DelINode(fd, data, iNode);
}

#endif