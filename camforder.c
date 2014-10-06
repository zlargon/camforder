#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#include "noly.h"
#include "log.h"

#define BUF_LEN 4096

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
} CVR;

void send_http_post(CVR *cvr) {
    char http_post[2048] = {0};

    // set body, and get the length
    char body[256] = {0};
    int body_len = sprintf(body, "HOST=%s&PORT=%d&PATH=%s&ID=%s&UID=%s", cvr->ipcam_addr, cvr->ipcam_port, cvr->rtsp_path, cvr->device_id, cvr->uid);

    // set header to http_post
    int header_len = sprintf(http_post,
        "POST /info HTTP/1.0\r\n"
        "CSeq: 1\r\n"
        "User-Agent: Camforder\r\n"
        "x-sessioncookie: d4db051ca2ceb40be4c2958\r\n"
        "Accept: application/x-rtsp-tunnelled\r\n"
        "Pragma: no-cache\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Length:%d\r\n"
        "\r\n",
        body_len
    );

    // set body to http_post
    memcpy(http_post + header_len, body, body_len);

    printf("\n\nHTTP Post Content (length = %d)\n", header_len + body_len);
    puts("=====================================================================");
    printf("%s\n", http_post);
    puts("=====================================================================\n");

    // send http post
    send(cvr->server_fd, http_post, header_len + body_len, 0);
}

int crv_check_config(CVR *cvr) {
    if (cvr == NULL) {
        LOG(LOG_ERROR, "CVR should not be NULL\n");
        return -1;
    }

    if (cvr->server_addr == NULL || strlen(cvr->server_addr) <= 0) {
        LOG(LOG_ERROR, "Server address should not be NULL or empty\n");
        return -1;
    }

    if (cvr->server_port < 1 || cvr->server_port > 65535) {
        LOG(LOG_ERROR, "Server port (%d) is out of range 1 ~ 65535\n", cvr->server_port);
        return -1;
    }

    if (cvr->ipcam_addr == NULL || strlen(cvr->ipcam_addr) <= 0) {
        LOG(LOG_ERROR, "Media address should not be NULL or empty\n");
        return -1;
    }

    if (cvr->ipcam_port < 1 || cvr->ipcam_port > 65535) {
        LOG(LOG_ERROR, "Media port (%d) is out of range 1 ~ 65535\n", cvr->ipcam_port);
        return -1;
    }

    return 0;
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
    if (cvr->ipcam_fd == -1) {
        LOG(LOG_ERROR, "Connect to IPCAM failure\n");
        return -1;
    }

    LOG(LOG_INFO, "Connected to IPCAM success\n");
    return 0;
}

int disconnect_ipcam(CVR *cvr) {
    if (cvr->ipcam_fd > 0) {
        close(cvr->ipcam_fd);
    }
    return 0;
}

int connect_server(CVR *cvr) {
    cvr->server_fd = noly_tcp_connect(cvr->server_addr, cvr->server_port);
    if (cvr->server_fd == -1) {
        LOG(LOG_ERROR, "Connect to CVR server failure\n");
        return -1;
    }

    LOG(LOG_INFO, "Connected to CVR server success\n");
    return 0;
}

int disconnect_server(CVR *cvr) {
    if (cvr->server_fd > 0) {
        close(cvr->server_fd);
    }
    return 0;
}

int run(CVR *cvr) {
    if (cvr == NULL) {
        LOG(LOG_FATAL, "No setting pass to run server\n");
        return -1;
    }

    // connect ipcam and CVR server
    if (connect_ipcam(cvr) != 0 || connect_server(cvr) != 0) {
        return -1;
    }

    // http post
    send_http_post(cvr);

    for (;;) {
        // config FD set, and get the max fd
        fd_set fs;
        FD_ZERO(&fs);
        int max = 0;
        if (cvr->ipcam_fd > 0) {
            FD_SET(cvr->ipcam_fd, &fs);
            max = max > cvr->ipcam_fd ? max : cvr->ipcam_fd;
        }

        if (cvr->server_fd > 0) {
            FD_SET(cvr->server_fd, &fs);
            max = max > cvr->server_fd ? max : cvr->server_fd;
        }

        // config timeval
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        // select
        int ret = select(max + 1, &fs, NULL, NULL, &tv);

        /* 1. select = 0, timeout */
        if (ret == 0) {
            LOG(LOG_DEBUG, "timeout\n");
            continue;
        }

        /* 2. select < 0, TODO: implement the error handle */
        if (ret < 0) {
            LOG(LOG_ERROR, "errno %s (%d)\n", strerror(errno), errno);
            return -1;
        }

        /* 3. select > 0 */

        // forward ipcam_fd to server_fd
        if (FD_ISSET(cvr->ipcam_fd, &fs)) {
            char buf[BUF_LEN] = {0};
            int len = recv(cvr->ipcam_fd, buf, BUF_LEN, 0);
            // LOG(LOG_DEBUG, "Read from ipcam %d bytes:\n%s\n", len, buf);
            send(cvr->server_fd, buf, len, 0);
        }

        // forward server_fd to ipcam_fd
        if (FD_ISSET(cvr->server_fd, &fs)) {
            char buf[BUF_LEN] = {0};
            int len = recv(cvr->server_fd, buf, BUF_LEN, 0);
            // LOG(LOG_DEBUG, "Read from server %d bytes:\n%s\n", len, buf);
            send(cvr->ipcam_fd, buf, len, 0);
        }
    }

    return 0;
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
    if (crv_check_config(&cvr) == -1) {
        LOG(LOG_FATAL, "camera forwarder setting error\n");
        return -1;
    }
    run(&cvr);
    return 0;
}
