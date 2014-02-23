/* A simple http server example using RDS-EVENT-LIB, just echo the server's time to client.
 *
 * Copyright (c) 2014, pandyxu <pandyxu at outlook dot com>
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <syslog.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netdb.h>


#include "ae.h"
#include "anet.h"
#include "zmalloc.h"

#ifndef DBGLOG
#define DBGLOG(format, ...) \
    printf("[%s:%d:%s]: "format"\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#endif

#ifndef ERRLOG
#define ERRLOG(format, ...) \
    fprintf(stderr, "[ERROR][%s:%d]: "format"\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#define SHTTPSVR_EVENTLOOP_FDSET_INCR 128
#define SHTTPSVR_IP_STR_LEN INET6_ADDRSTRLEN
#define SHTTPSVR_IOBUF_LEN (1024*16)

struct web_server {
    aeEventLoop *el;
    int max_process_client;
    int tcp_backlog;
    int tcpkeepalive;
    int hz;
    char backgroundBuf[64];
};

static struct web_server server;

typedef struct client_data_s {
    int fd;
    char readBuf[SHTTPSVR_IOBUF_LEN];
    int readBufPos;
    char writeBuf[SHTTPSVR_IOBUF_LEN];
    int writeBufPos;
    int sentlen;
}client_data_t;

client_data_t* create_client(int fd)
{
    client_data_t *c = zmalloc(sizeof(client_data_t));
    if (NULL == c)
    {
        ERRLOG("memory not enough.");
        return NULL;
    }
    memset(c, 0, sizeof(client_data_t));

    c->fd = fd;
    c->writeBufPos = 0;
    c->sentlen = 0;
    return c;
}

void free_client(client_data_t* c) {
    if (NULL == c) {
        return;
    }

    if (c->fd != -1) {
        aeDeleteFileEvent(server.el,c->fd,AE_READABLE);
        aeDeleteFileEvent(server.el,c->fd,AE_WRITABLE);
        close(c->fd);
    }
    zfree(c);
}

void tWriteProc(struct aeEventLoop* eventLoop, int fd, void* clientData, int mask) {
    char pageBuf[512] = {0};
    char contentBuf[256] = {0};
    client_data_t *c = (client_data_t *)clientData;

    snprintf(contentBuf,
        sizeof(contentBuf),
        "<html>Hello, rds-event-lib(redis event library) test.<br /><br />%s</html>",
        server.backgroundBuf
        );

    snprintf(pageBuf, sizeof(pageBuf),
        "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n%s",
        strlen(contentBuf), contentBuf);
    int data_size = strlen(pageBuf);

    size_t available = sizeof(c->writeBuf)-c->writeBufPos;
    if (data_size > available)
    {
        ERRLOG("writeBuf not enough.");
        aeDeleteFileEvent(eventLoop, fd, AE_WRITABLE);
        free_client(c);
        return;
    }

    memcpy(c->writeBuf+c->writeBufPos, pageBuf, data_size);
    c->writeBufPos += data_size;

    int nwritten = 0;
    while(c->writeBufPos > 0) {
        nwritten = write(fd, c->writeBuf + c->sentlen, c->writeBufPos - c->sentlen);
        if (nwritten <= 0)
        {
            break;
        }
        c->sentlen += nwritten;

        /* If the buffer was sent, set writeBufPos to zero to continue with
         * the remainder of the reply. */
        if (c->sentlen == c->writeBufPos) {
            c->writeBufPos = 0;
            c->sentlen = 0;
        }
    }

    if (nwritten == -1) {
        if (errno == EAGAIN) {
            nwritten = 0;
        } else {
            ERRLOG("Error writing to client: %s", strerror(errno));
            aeDeleteFileEvent(eventLoop, fd, AE_WRITABLE);
            free_client(c);
            return;
        }
    }

    aeDeleteFileEvent(eventLoop, fd, AE_WRITABLE);
    free_client(c);
}

void tReadProc(struct aeEventLoop* eventLoop, int fd, void* clientData, int mask) {
    client_data_t *c = (client_data_t *)clientData;
    char buf[SHTTPSVR_IOBUF_LEN] = {0};
    int nread = 0;

    for(;;) {
        nread = read(fd, c->readBuf + c->readBufPos, sizeof(c->readBuf) - c->readBufPos);
        if (nread == -1) {
            if (errno != EAGAIN) {
                ERRLOG("Read data from fd[%d] error.", fd);
                free_client(clientData);
                return;
            } else {
                break;
            }
        } else if (nread == 0) {
            DBGLOG("Client closed connection.");
            free_client(clientData);
            return;
        } else if (nread > 0) {
            c->readBufPos += nread;
        }
    }

    if (c->readBufPos > sizeof(c->readBuf) - 1)
    {
        ERRLOG("Read request from fd[%d] error.", fd);
        free_client(clientData);
        return;
    }

    c->readBuf[c->readBufPos] = '\0';
    DBGLOG("Current read buffer: \n%s", c->readBuf);
    char *http_head_tail_pos = (strstr(c->readBuf, "\r\n\r\n"));
    if (NULL == http_head_tail_pos)
    {
        /* continue read when next file event */
        return;
    }

    /* copy data for next request to the header of readBuf */
    size_t dataLengthForNextRequest = (c->readBuf + c->readBufPos) - (http_head_tail_pos + 4);
    /*
    * The  memmove()  function copies n bytes from memory area src to memory area dest.
    * The memory areas may overlap: copying takes place as though the bytes in
    * src are first copied into a temporary array that does not overlap src or dest,
    * and the bytes are then copied from the temporary array to dest.
    */
    memmove(c->readBuf, http_head_tail_pos + 4, dataLengthForNextRequest);
    c->readBufPos = dataLengthForNextRequest;

    if (aeCreateFileEvent(eventLoop, fd, AE_WRITABLE, tWriteProc, clientData) == AE_ERR) {
        ERRLOG("aeCreateFileEvent error.");
        return;
    }
}

void tAcceptProc(struct aeEventLoop* eventLoop, int fd, void* clientData, int mask) {
    int cport;
    char cip[SHTTPSVR_IP_STR_LEN];

    int cfd = anetTcpAccept(NULL, fd, cip, sizeof(cip), &cport);

    if (cfd != -1) {
        anetNonBlock(NULL, cfd);
        anetEnableTcpNoDelay(NULL, cfd);
        if (server.tcpkeepalive) {
            anetKeepAlive(NULL, cfd, server.tcpkeepalive);
        }

        client_data_t* c = create_client(cfd);
        if (aeCreateFileEvent(eventLoop, cfd, AE_READABLE,
                              tReadProc, c) == AE_ERR) {
            DBGLOG("aeCreateFileEvent error.", cip, cport);
            free_client(c);
            return;
        }
    }
    DBGLOG("Accepted %s:%d", cip, cport);
    return;
}


int tBackgroundTask(struct aeEventLoop* eventLoop, long long id, void* clientData) {
    time_t t_tmp;
    struct tm *t_now;
    time(&t_tmp);
    t_now = localtime(&t_tmp);
    return snprintf(server.backgroundBuf, sizeof(server.backgroundBuf),
        "Current server time: %4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d",
        t_now->tm_year + 1900,
        t_now->tm_mon + 1,
        t_now->tm_mday,
        t_now->tm_hour,
        t_now->tm_min,
        t_now->tm_sec);
    return 1000/server.hz;
}

void tEventFinalizerProc(struct aeEventLoop* eventLoop, void* clientData) {
    // DBGLOG("aeEventFinalizerProc.");
}

void tBeforeSleepProc(struct aeEventLoop* eventLoop) {
    // DBGLOG("BeforeSleepProc.");
}

void sighandler(int sig) {
    exit(0);
}

void initServer(void) {
    server.max_process_client = 1000;
    server.tcp_backlog = 20;
    server.tcpkeepalive = 0;
    server.hz = 10;
}

int main(int argc, char** argv) {
    signal(SIGABRT, &sighandler);
    signal(SIGTERM, &sighandler);
    signal(SIGINT, &sighandler);

    initServer();

    aeEventLoop* eventLoop = aeCreateEventLoop(server.max_process_client + SHTTPSVR_EVENTLOOP_FDSET_INCR);
    server.el = eventLoop;

    int listen_socket = anetTcpServer(NULL, 80, NULL, server.tcp_backlog);
    anetNonBlock(NULL, listen_socket);

    if (listen_socket > 0
        && AE_ERR == aeCreateFileEvent(eventLoop, listen_socket, AE_READABLE, tAcceptProc, NULL)) {
        ERRLOG("aeCreateFileEvent error!");
        exit(1);
    }

    aeSetBeforeSleepProc(eventLoop, tBeforeSleepProc);
    if (aeCreateTimeEvent(eventLoop, 1, tBackgroundTask, NULL, NULL) == AE_ERR) {
        ERRLOG("Can't create the serverCron time event.");
        exit(1);
    }

    aeMain(eventLoop);
    aeDeleteEventLoop(eventLoop);
    return 0;
}

