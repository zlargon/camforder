#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>

#include "noly.h"
#include "log.h"

typedef struct {
    int                 server_fd;
    char                server_addr[32];
    int                 server_port;
    int                 ipcam_fd;
    char                ipcam_addr[32];
    int                 ipcam_port;
    char                device_id[32];
    char                rtsp_path[64];
    char                uid[32];
    struct sockaddr_in  saddr;
    struct sockaddr_in  maddr;
    int                 ipcam_status;
    int                 server_status;
} CVR;

void send_http_post(CVR *cvr) {
    char http_post[1024];
    char body[256];
    int len = sprintf(body, "HOST=%s&PORT=%d&PATH=%s&ID=%s&UID=%s", cvr->ipcam_addr, cvr->ipcam_port, cvr->rtsp_path, cvr->device_id, cvr->uid);
    printf("%s\n", body);
    int tlen = sprintf(http_post, "POST /info HTTP/1.0\r\nCSeq: 1\r\nUser-Agent: Camforder\r\n"
            "x-sessioncookie: d4db051ca2ceb40be4c2958\r\n"
            "Accept: application/x-rtsp-tunnelled\r\n"
            "Pragma: no-cache\r\n"
            "Cache-Control: no-cache\r\n"
            "Content-Length:%d\r\n"
            "\r\n", len);
    char *ptr = http_post + tlen;
    memcpy(ptr, body, len);
    printf("%s\n", http_post);
    send(cvr->server_fd, http_post, len + tlen, 0);
}

int config_checking(CVR *cvr) {
    int ret = 0;
    if (strlen(cvr->server_addr) == 0) {
        LOG(LOG_ERROR, "Server address should not empty\n");
        ret = -1;
    }
    if (cvr->server_port == 0 || cvr->server_port > 65534) {
        LOG(LOG_ERROR, "Server port out of range\n");
        ret = -1;
    }
    if (strlen(cvr->ipcam_addr) == 0) {
        LOG(LOG_ERROR, "Media address should not empty\n");
        ret = -1;
    }
    if (cvr->ipcam_port == 0 || cvr->ipcam_port > 65534) {
        LOG(LOG_ERROR, "Media port out of range\n");
        ret = -1;
    }
    return ret;
}

void dump_config(CVR *cvr) {
    LOG(LOG_INFO, "Connect to CVR server %s:%d\n", cvr->server_addr, cvr->server_port);
    LOG(LOG_INFO, "Connect to ipcam server rtsp://%s:%d/%s\n", cvr->ipcam_addr, cvr->ipcam_port, cvr->rtsp_path);
}

void print_usage(char *prog) {
    printf("Run %s with following option\n", prog);
    printf("-s  the CVR server address 122.146.86.11\n");
    printf("-p  the CVR server port 8080\n");
    printf("-h  the ipcam server address 192.168.1.1\n");
    printf("-p  the ipcam server port 554\n");
    printf("-d  the device identity\n");
    printf("-l  the RTSP path\n");
}

int connect_ipcam(CVR *cvr) {
    cvr->ipcam_fd = noly_tcp_connect(cvr->ipcam_addr, cvr->ipcam_port);
    if (cvr->ipcam_fd > 0) {
        LOG(LOG_INFO, "Connected to IPCAM\n");
        // cvr->ipcam_status;
        return 0;
    }
    LOG(LOG_ERROR, "Connect to IPCAM failure\n");
    return -1;
}

int disconnect_ipcam(CVR *cvr) {
    if (cvr->ipcam_fd > 0) {
        close(cvr->ipcam_fd);
    }
    return 0;
}

int connect_server(CVR *cvr) {
    cvr->server_fd = noly_tcp_connect(cvr->server_addr, cvr->server_port);
    if (cvr->server_fd > 0) {
        LOG(LOG_INFO, "Connected to CVR server\n");
        // cvr->server_status;
        return 0;
    }
    LOG(LOG_ERROR, "Connect to CVR server failure\n");
    return -1;
}

int disconnect_server(CVR *cvr) {
    if (cvr->server_fd > 0) {
        close(cvr->server_fd);
    }
    return 0;
}

void *run(void *data) {
    int r = 0;
    CVR *cvr = (CVR *)data;

    if (cvr == NULL) {
        LOG(LOG_FATAL, "No setting pass to run server\n");
        return NULL;
    }

    fd_set fs;
    while (1) {
        char buf[4096];
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        if (cvr->ipcam_fd <= 0) {
            connect_ipcam(cvr);
        }
        if (cvr->server_fd <= 0) {
            if (cvr->ipcam_status == 1) { //when ipcam already running
                disconnect_ipcam(cvr);
            }
            if (connect_server(cvr) == 0) {
                send_http_post(cvr);
            }
        }
        int max = 0;
        FD_ZERO(&fs);
        if (cvr->ipcam_fd > 0) {
            FD_SET(cvr->ipcam_fd, &fs);
            max = max > cvr->ipcam_fd ? max : cvr->ipcam_fd;
        }
        if (cvr->server_fd > 0) {
            FD_SET(cvr->server_fd, &fs);
            max = max > cvr->server_fd ? max : cvr->server_fd;
        }
        int ret = select(max + 1, &fs, NULL, NULL, &tv);
        if (ret == 0) {
            LOG(LOG_DEBUG, "timeout\n");
        } else {
            if (FD_ISSET(cvr->ipcam_fd, &fs)) {
                //read from ipcam
                memset(buf, 0, 4096);
                int len = recv(cvr->ipcam_fd, buf, 4096, 0);
                //LOG(LOG_DEBUG, "Read from ipcam %d bytes:\n%s\n", len, buf);
                send(cvr->server_fd, buf, len, 0);
            }
            if (FD_ISSET(cvr->server_fd, &fs)) {
                memset(buf, 0, 4096);
                int len = recv(cvr->server_fd, buf, 4096, 0);
                //LOG(LOG_DEBUG, "Read from server %d bytes:\n%s\n", len, buf);
                send(cvr->ipcam_fd, buf, len, 0);
                //read from ipcam
            }
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    LOG(LOG_INFO, "Start camforder...\n");

    CVR cvr;
    memset(&cvr, 0, sizeof(cvr));

    char param;
    while ((param = getopt(argc, argv, "s:p:h:o:d:l:u:")) != -1) {
        switch (param) {
            case 's':
                strncpy(cvr.server_addr, optarg, 32);
                break;
            case 'p':
                cvr.server_port = atoi(optarg);
                break;
            case 'h':
                strncpy(cvr.ipcam_addr, optarg, 32);
                break;
            case 'o':
                cvr.ipcam_port = atoi(optarg);
                break;
            case 'd':
                strncpy(cvr.device_id, optarg, 32);
                break;
            case 'l':
                strncpy(cvr.rtsp_path, optarg, 64);
                break;
            case 'u':
                strncpy(cvr.uid, optarg, 32);
                break;
            default:
                print_usage(argv[0]);
                return -1;
                break;
        }
    }

    dump_config(&cvr);
    if (config_checking(&cvr) == -1) {
        LOG(LOG_FATAL, "camera forwarder setting error\n");
        return -1;
    }
    run(&cvr);
    return 0;
}
