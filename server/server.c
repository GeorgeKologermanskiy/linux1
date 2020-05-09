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

#include "config.c"
#include "start_stop_fs.c"
#include "dir.c"
#include "file.c"
#include "ls.c"
#include "rm.c"
#include "file_open_close.c"
#include "file_read_write.c"
void Daemon();

int main(int argc, char **argv)
{
    pid_t pid = fork();
    if (-1 == pid)
    {
        printf("Cant fork, close\n");
        return 0;
    }
    if (0 != pid)
    {
        printf("Daemon created with pid: %d\n", pid);
        return 0;
    }
    setsid();
    chdir("/");
    fclose(stdin);
    fclose(stdout);
    fclose(stderr);
    Daemon(argc, argv);
    return 0;
}

int sfd;
FILE *logfile;
FSMetaData data;
FILE* fd;

void handle_sigint()
{
    fprintf(logfile, "Got sigint\n");
    stop_fs(fd, &data);
    close(sfd);
    fclose(logfile);
    exit(0);
}

// have to free
char * read_buff(int client_fd, int *len)
{
    int r = read(client_fd, len, sizeof(int));
    if (r < sizeof(int))
    {
        return NULL;
    }
    char *buff = (char*)malloc((*len + 1) * sizeof(char));
    r = read(client_fd, buff, (*len) * sizeof(char));
    if (r < (*len) * sizeof(char))
    {
        free(buff);
        return NULL;
    }
    buff[*len] = 0;
    return buff;
}

void Daemon(int argc, char **argv)
{
    struct sigaction action_int;
    memset(&action_int, 0, sizeof(action_int));
    action_int.sa_handler = handle_sigint;
    action_int.sa_flags = SA_RESTART;
    sigaction(SIGINT, &action_int, NULL);

    logfile = fopen("/home/george/logfile.log", "w+");
    if (NULL == logfile)
    {
        return;
    }

    fd = start_fs(argc, argv, &data);
    if (NULL == fd)
    {
        fprintf(logfile, "Unable to start FS\n");
        return;
    }

    sfd = socket(PF_INET, SOCK_STREAM, 0);
    if (-1 == sfd)
    {
        fprintf(logfile, "Dont open socket\n");
        fclose(logfile);
        return;
    }

    fprintf(logfile, "socket: %d\n", sfd);

    struct sockaddr_in name;
    name.sin_family = AF_INET;
    name.sin_addr.s_addr = INADDR_ANY;
    name.sin_port = htons(PORT_CON);
    if(-1 == bind(sfd, (const struct sockaddr *)(&name), sizeof(name)))
    {
        fprintf(logfile, "Don`t bind to socket\n");
        goto end;
    }

    fprintf(logfile, "Binded\n");

    if (0 != listen(sfd, 5))
    {
        fprintf(logfile, "Dont start listen connections\n");
        goto end;
    }
    
    fprintf(logfile, "Start listen\n");

    for(;;)
    {
        fprintf(logfile, "Wait for accept\n");

        struct sockaddr_in client_name;
        socklen_t size;
        int client_fd = accept(sfd, (struct sockaddr *)(&client_name), &size);
        if (client_fd < 0)
        {
            fprintf(logfile, "Bad connect try\n");
            continue;
        }

        fprintf(logfile, "Accepted\n");

        for(;;)
        {
            OperationType type;
            if (read(client_fd, &type, sizeof(OperationType)) < sizeof(OperationType))
            {
                break;
            }

            if (OT_CLOSE_CONNECTION == type)
            {
                fprintf(logfile, "Close connection\n");
                break;
            }
            
            if (OT_LS == type)
            {
                fprintf(logfile, "ls\n");
                int len;
                if (read(client_fd, &len, sizeof(int)) < sizeof(int))
                {
                    continue;
                }
                char *dirname = NULL;
                if (len > 0)
                {
                    dirname = (char*)malloc((len + 1) * sizeof(char));
                    if (read(client_fd, dirname, len * sizeof(char)) < len * sizeof(char))
                    {
                        free(dirname);
                        continue;
                    }
                    dirname[len] = 0;
                }

                int count = ls(fd, &data, dirname, 0, NULL);
                write(client_fd, &count, sizeof(int));
                if (count <= 0)
                {
                    free(dirname);
                    continue;
                }

                lsRetItem* items = (lsRetItem*)malloc(count * sizeof(lsRetItem));
                count = ls(fd, &data, dirname, 0, items);
                write(client_fd, items, count * sizeof(lsRetItem));

                free(dirname);
                free(items);
                continue;
            }

            if (OT_MAKE_DIR == type)
            {
                int dirnamelen;
                char *dirname = read_buff(client_fd, &dirnamelen);
                if (NULL == dirname)
                {
                    continue;
                }
                fprintf(logfile, "mkdir %s\n", dirname);

                mkItemRet ret;
                make_dir(fd, &data, dirname, 0, &ret);
                free(dirname);
                write(client_fd, &ret, sizeof(mkItemRet));
                continue;
            }

            if (OT_MAKE_FILE == type)
            {
                int filenamelen;
                char *filename = read_buff(client_fd, &filenamelen);
                if (NULL == filename)
                {
                    continue;
                }
                
                fprintf(logfile, "mkfile %s\n", filename);

                mkItemRet ret;
                make_file(fd, &data, filename, 0, &ret);
                free(filename);
                write(client_fd, &ret, sizeof(mkItemRet));
                continue;
            }
        
            if (OT_RM == type)
            {
                int itemlen;
                char *item = read_buff(client_fd, &itemlen);
                if (NULL == item)
                {
                    continue;
                }

                fprintf(logfile, "rm %s\n", item);

                rmRet ret;
                rm(fd, &data, item, 0, &ret);
                free(item);
                write(client_fd, &ret, sizeof(rmRet));
                continue;
            }
        
            if (OT_OPEN_FILE == type)
            {
                int filenamelen;
                char *filename = read_buff(client_fd, &filenamelen);
                if (NULL == filename)
                {
                    continue;
                }
                OpenFileType rtype;
                if (read(client_fd, &rtype, sizeof(OpenFileType)) < sizeof(OpenFileType))
                {
                    free(filename);
                    continue;
                }
                fprintf(logfile, "Open file %s ; type - %d\n", filename, rtype);
                int ret = open_file(fd, &data, filename, rtype, 0);
                write(client_fd, &ret, sizeof(int));
                continue;
            }

            if (OT_CLOSE_FILE == type)
            {
                int fs_fd;
                if (read(client_fd, &fs_fd, sizeof(int)) < sizeof(int))
                {
                    continue;
                }
                fprintf(logfile, "Close file %d\n", fs_fd);

                int ret = close_file(fd, &data, fs_fd);
                write(client_fd, &ret, sizeof(int));
                continue;
            }
        
            if (OT_READ_FILE == type)
            {
                int fs_fd;
                int from;
                int count;
                if (read(client_fd, &fs_fd, sizeof(int)) < sizeof(int))
                {
                    continue;
                }
                if (read(client_fd, &from, sizeof(int)) < sizeof(int))
                {
                    continue;
                }
                if (read(client_fd, &count, sizeof(int)) < sizeof(int))
                {
                    continue;
                }
                fprintf(logfile, "Read file fd: %d, from: %d, count: %d\n", fs_fd, from, count);
                char *buff = (char*) malloc(count * sizeof(char));
                memset(buff, 0, count * sizeof(char));
                int ret = read_file(fd, &data, fs_fd, buff, from, count);

                write(client_fd, &ret, sizeof(int));
                ret = strlen(buff);
                write(client_fd, &ret, sizeof(int));
                write(client_fd, buff, ret * sizeof(char));
                free(buff);
                continue;
            }
        
            if (OT_WRITE_FILE == type)
            {
                int fs_fd;
                if (read(client_fd, &fs_fd, sizeof(int)) < sizeof(int))
                {
                    continue;
                }
                int len;
                char *buff = read_buff(client_fd, &len);
                if (NULL == buff)
                {
                    continue;
                }

                fprintf(logfile, "write_file fd: %d, buff: %s\n", fs_fd, buff);

                int ret = write_file(fd, &data, fs_fd, buff, len);
                write(client_fd, &ret, sizeof(int));
                free(buff);
            }
        }

        close(client_fd);
    }
end:
    stop_fs(fd, &data);
    close(sfd);
    fclose(logfile);
}
