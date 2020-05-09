#include <stdio.h> 
#include <string.h> 
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#include "../server/config.c"
#include "../server/structs.h"
#include "../server/ret_values.h"

int sfd;

void handle_sigint()
{
    OperationType oType = OT_CLOSE_CONNECTION;
    write(sfd, &oType, sizeof(OperationType));
    printf("\n");
    close(sfd);
    exit(0);
}

int main(int argc, char **argv)
{
    struct sigaction action_int;
    memset(&action_int, 0, sizeof(action_int));
    action_int.sa_handler = handle_sigint;
    action_int.sa_flags = SA_RESTART;
    sigaction(SIGINT, &action_int, NULL);

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT_CON);

    if (inet_pton(AF_INET, "localhost", &server_addr.sin_addr) < 0)
    {
        printf("inet_pton problem\n");
        goto end;
    }

    // new socket
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0)
    {
        printf("Socket don`t created\n");
        return 0;
    }

    // connect
    if (connect(sfd, (struct sockaddr *)(&server_addr), sizeof(server_addr)) < 0)
    {
        printf("Cant connect to server\n");
        goto end;
    }

    char *line = NULL;
    size_t line_len = 0;
    OperationType oType;
    for(;;)
    {
        printf("\n>");
        int res = getline(&line, &line_len, stdin);
        if (res <= 0)
        {
            printf("Unable to read line\n");
            continue;
        }
        line[res - 1] = 0;

        if (strncmp(line, "ls", 2) == 0)
        {
            // parse and send
            oType = OT_LS;
            write(sfd, &oType, sizeof(OperationType));
            int len = 0;
            if (res > 3)
            {
                len = strlen(line + 3);
                write(sfd, &len, sizeof(int));
                write(sfd, line + 3, len * sizeof(char));
            }
            else
            {
                write(sfd, &len, sizeof(int));
            }

            // receive and print
            int count;
            if (read(sfd, &count, sizeof(int)) < sizeof(int))
            {
                printf("Server problem\n");
                continue;
            }
            
            if (-1 == count)
            {
                printf("There is a file in directory path\n");
                continue;
            }
            if (-2 == count)
            {
                printf("Directory not found\n");
                continue;
            }
            if (0 == count)
            {
                continue;
            }

            // if need to write something
            lsRetItem *ret = (lsRetItem*)malloc(count * sizeof(lsRetItem));
            if (read(sfd, ret, count * sizeof(lsRetItem)) < count * sizeof(lsRetItem))
            {
                free(ret);
                printf("Server problem\n");
                continue;
            }
            
            for (int pos = 0; pos < count; ++pos)
            {
                lsRetItem *curr = ret + pos;
                if (IT_FILE == curr->item.type)
                {
                    printf("FILE:\t\t iNode - %d\t name - %s\n", curr->item.iNodeLoc, curr->item.name);
                }
                if (IT_DIRECTORY == curr->item.type)
                {
                    printf("DIRECTORY:\t iNode - %d\t name - %s\n", curr->item.iNodeLoc, curr->item.name);
                }
            }
            free(ret);
            continue;
        }

        if (strncmp(line, "rm", 2) == 0)
        {
            oType = OT_RM;
            int len = strlen(line + 3);
            // send
            write(sfd, &oType, sizeof(OperationType));
            write(sfd, &len, sizeof(int));
            write(sfd, line + 3, len * sizeof(char));

            // receive
            rmRet ret;
            if (read(sfd, &ret, sizeof(rmRet)) < sizeof(rmRet))
            {
                printf("Server problem\n");
                continue;
            }

            // print result
            if (ret.isFileDirProblem)
            {
                printf("There is file in remove path, but have to be directory\n");
            }
            else if(!ret.isFoundItem)
            {
                printf("Item not found\n");
            }
            else if(ret.isFile)
            {
                printf("File %s deleted\n", line + 3);
            }
            else
            {
                printf("Directory %s deleted\n", line + 3);
            }
            continue;
        }

        if (strncmp(line, "exit", 4) == 0)
        {
            break;
        }

        if (strncmp(line, "mkdir", 5) == 0)
        {
            oType = OT_MAKE_DIR;
            int len = strlen(line + 6);

            // send
            write(sfd, &oType, sizeof(OperationType));
            write(sfd, &len, sizeof(int));
            write(sfd, line + 6, len * sizeof(char));

            // receive
            mkItemRet ret;
            if (read(sfd, &ret, sizeof(mkItemRet)) < sizeof(mkItemRet))
            {
                printf("Server problem\n");
                continue;
            }

            // print result
            if (ret.isBadOperation)
            {
                printf("Problem with operation\n");
            }
            else if (ret.isExistDir)
            {
                printf("This directory is exist\n");
            }
            else if (ret.isExistFile)
            {
                printf("There is file with this name\n");
            }
            else
            {
                printf("Directory %s is created\n", line + 6);
            }
            continue;
        }

        if (strncmp(line, "mkfile", 6) == 0)
        {
            oType = OT_MAKE_FILE;
            int len = strlen(line + 7);

            // send
            write(sfd, &oType, sizeof(OperationType));
            write(sfd, &len, sizeof(int));
            write(sfd, line + 7, len * sizeof(char));

            // receive
            mkItemRet ret;
            if (read(sfd, &ret, sizeof(mkItemRet)) < sizeof(mkItemRet))
            {
                printf("Server problem\n");
                continue;
            }

            // print result
            if (ret.isBadOperation)
            {
                printf("Problem with operation\n");
            }
            else if (ret.isExistDir)
            {
                printf("There is directory with the same name\n");
            }
            else if (ret.isExistFile)
            {
                printf("There is file with this name or file in name which have to be directory\n");
            }
            else
            {
                printf("File %s is created\n", line + 7);
            }
            continue;
        }

        if (strncmp(line, "open_file", 9) == 0)
        {
            oType = OT_OPEN_FILE;

            // parse args
            int typePos = 10;
            while(line[typePos] != ' ' && line[typePos] != 0)
            {
                ++typePos;
            }
            if (0 == line[typePos])
            {
                printf("Invalid syntax for open_file\n");
                continue;
            }
            line[typePos] = 0;
            ++typePos;
            OpenFileType type = OFT_READ;
            if (line[typePos] == 'r')
            {
                type = OFT_READ;
            }
            else if(line[typePos] == 'w')
            {
                type = OFT_WRITE;
            }
            else
            {
                printf("Invalid open file type\n");
                continue;
            }

            // send
            int len = strlen(line + 10);
            write(sfd, &oType, sizeof(OperationType));
            write(sfd, &len, sizeof(int));
            write(sfd, line + 10, len * sizeof(char));
            write(sfd, &type, sizeof(OpenFileType));

            // receive
            int ret = 0;
            if (read(sfd, &ret, sizeof(int)) < sizeof(int))
            {
                printf("Server problem\n");
                continue;
            }
            
            // print result
            if (-3 == ret)
            {
                printf("There is a file in path, which have to be directory\n");
            }
            else if (-2 == ret)
            {
                printf("File not found for open\n");
            }
            else if (-1 == ret)
            {
                printf("All fd are used. Close one and retry\n");
            }
            else if (OFT_READ == type)
            {
                printf("FD for read: %d\n", ret);
            }else
            {
                printf("FD for write: %d\n", ret);
            }
            continue;
        }

        if (strncmp(line, "read_file", 9) == 0)
        {
            oType = OT_READ_FILE;

            // parse
            int pos;
            int from;
            int count;
            sscanf(line + 10, "%d %d %d", &pos, &from, &count);

            // send
            write(sfd, &oType, sizeof(OperationType));
            write(sfd, &pos, sizeof(int));
            write(sfd, &from, sizeof(int));
            write(sfd, &count, sizeof(int));

            // receive
            int ret = 0;
            if (read(sfd, &ret, sizeof(int)) < sizeof(int))
            {
                printf("Server problem\n");
                continue;
            }
            int size = 0;
            if (read(sfd, &size, sizeof(int)) < sizeof(int))
            {
                printf("Server problem\n");
                continue;
            }
            char *buff = (char*)malloc(sizeof(char) * (size + 1));
            if (read(sfd, buff, size * sizeof(char)) < size * sizeof(char))
            {
                free(buff);
                printf("Server problem\n");
                continue;
            }
            buff[size] = 0;

            // print result
            if (-2 == ret)
            {
                printf("Incorrect FD\n");
            }
            if (-1 == ret)
            {
                printf("FD %d is opened for write\n", pos);
            }
            if (0 == ret)
            {
                printf("Result: %s\n", buff);
            }
            free(buff);
            continue;
        }

        if (strncmp(line, "close_file", 10) == 0)
        {
            oType = OT_CLOSE_FILE;

            // parse
            int pos;
            sscanf(line + 11, "%d", &pos);

            // send
            write(sfd, &oType, sizeof(OperationType));
            write(sfd, &pos, sizeof(int));

            // receive
            int ret = 0;
            if (read(sfd, &ret, sizeof(int)) < sizeof(int))
            {
                printf("Server problem\n");
                continue;
            }

            if (0 == ret)
            {
                printf("Incorrect FD\n");
            }
            else if (1 == ret)
            {
                printf("FD %d is closed\n", pos);
            }
            else
            {
                printf("Unknown behavior\n");
            }
            continue;
        }

        if (strncmp(line, "write_file", 10) == 0)
        {
            oType = OT_WRITE_FILE;

            // parse
            char *end;
            int pos = strtol(line + 11, &end, 10);
            int buff_start = 11;
            while(line[buff_start] != ' ' && line[buff_start] != 0)
            {
                ++buff_start;
            }
            ++buff_start;

            // send
            write(sfd, &oType, sizeof(OperationType));
            write(sfd, &pos, sizeof(int));
            int len = strlen(line + buff_start);
            write(sfd, &len, sizeof(int));
            write(sfd, line + buff_start, len * sizeof(char));

            // receive
            int ret;
            if (read(sfd, &ret, sizeof(int)) < sizeof(int))
            {
                printf("Server problem\n");
                continue;
            }

            // print result
            if (-2 == ret)
            {
                printf("Invalid FD: %d\n", pos);
                continue;
            }
            if (-1 == ret)
            {
                printf("Descriptor %d is opening fir read\n", pos);
                continue;
            }
            if (0 == ret)
            {
                printf("Complete %d bytes\n", len);
                continue;
            }
            printf("Undefined behavior\n");
            continue;
        }

        printf("Invalid syntax\n");
    }

end:
    oType = OT_CLOSE_CONNECTION;
    write(sfd, &oType, sizeof(OperationType));
    printf("Finish\n");
    close(sfd);
}