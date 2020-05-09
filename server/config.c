#ifndef CONFIG_C
#define CONFIG_C

#define PORT_CON 6823

typedef enum OperationType
{
    OT_LS,          // int dirnamelen, char *dirname                         |-----> int count, lsRetitem *items
    OT_RM,          // int itemlen, char *item                               |-----> rmRet
    OT_MAKE_DIR,    // int dirnamelen, char *dirname                         |-----> mkItemRet
    OT_MAKE_FILE,   // int filenamelen, char *filename                       |-----> mkItemRet
    OT_OPEN_FILE,   // int filenamelen, char *filename, OpenFileType type    |-----> int ret
    OT_READ_FILE,   // int fd, int from, int count                           |-----> int ret, int bufflen, char*buff
    OT_CLOSE_FILE,  // int fd                                                |-----> int ret
    OT_WRITE_FILE,  // int fd, int bufflen, char*buff                        |-----> int ret

    OT_CLOSE_CONNECTION
} OperationType;

#endif