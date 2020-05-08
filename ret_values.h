#ifndef RET_VALUES_H
#define RET_VALUES_H

#include "structs.h"

typedef struct mkItemRet
{
    int isExistDir;
    int isExistFile;
    int isBadOperation;
} mkItemRet;

typedef struct lsRetItem
{
    Item item;
} lsRetItem;

typedef struct rmRet
{
    int isFoundItem;
    int isFileDirProblem;
    int isFile;
} rmRet;

#endif