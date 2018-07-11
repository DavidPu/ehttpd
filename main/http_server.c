/* HTTP Server


   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifdef LINUX_BUILD
#include "mock_host.h"
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "host_debug.h"
#define TAG "httpd"

#else

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif /* MSG_NOSIGNAL */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"

const static char *TAG = "httpd";
#define static_assert(cond, msg) switch (0){case 0 : case !!(long)(cond):; }
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "http_server.h"
#include "djb2.h"

#define TASK_NAME "httpd"
#define TASK_STACK_WORDS 1024 * 10
#define TASK_PRIORITY 8
#define MAX_BACKLOG 32

#define INDEX_HTML                                                             \
    "<html><head><meta http-equiv=refresh content='0; url=/static/index.html' " \
    "/></head></html>"

static int write_all(int sockfd, const char *buf, size_t max_size);

/*
 * result of after study:
 * https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers
 */
struct resp_msg {
    int sockfd;
    size_t sz_hdr;  /* current header length. */
    size_t sz_body; /* current msg body length. */
    char header[MAX_RESP_HDR_SIZE];
    char body[MAX_RESP_BODY_SIZE];
};

static ssize_t inline avail_hdr(struct resp_msg *r)
{
    return MAX_RESP_HDR_SIZE - r->sz_hdr;
}

static ssize_t inline avail_body(struct resp_msg *r)
{
    return MAX_RESP_BODY_SIZE - r->sz_body;
}

const char *status_title(uint32_t status)
{
    switch (status) {
        /* supported: */
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        /* supported: */
        case 400: return "Bad Request";
        default: return "Not Found";
    }
}

status_t resp_start_header(struct resp_msg *r, uint32_t status)
{
    int size = 0;

    static_assert(MAX_RESP_HDR_SIZE > 64, "MAX_RESP_HDR_SIZE is too small.\n");

    memset(r->header, '\0', MAX_RESP_HDR_SIZE);
    size = snprintf(r->header, 64, "HTTP/1.1 %d %s\r\n", status,
                    status_title(status));

    r->sz_hdr = size;
    return STATUS_OK;
}

status_t resp_add_header(struct resp_msg *r, const char *name,
                         const char *value)
{
    int size      = 0;
    char buf[128] = {'\0'};
    char *p;

    p = r->header + r->sz_hdr;

    if (avail_hdr(r) <= 0) {
        return RESP_OOM_ERR;
    }
    size = snprintf(buf, sizeof(buf), "%s: %s\r\n", name, value);
    if (size >= avail_hdr(r)) {
        return RESP_OOM_ERR;
    }
    memcpy(p, buf, size);
    r->sz_hdr += size;
    return STATUS_OK;
}

static status_t resp_end_header(struct resp_msg *r)
{
    if (avail_hdr(r) < 2) {
        return RESP_OOM_ERR;
    }
    r->header[r->sz_hdr++] = '\r';
    r->header[r->sz_hdr++] = '\n';
    return STATUS_OK;
}

status_t resp_add_body(struct resp_msg *r, const char *data, size_t len)
{
    char *p;

    p = r->body + r->sz_body;
    if (avail_body(r) <= 0 || len > avail_body(r)) {
        return RESP_OOM_ERR;
    }
    memcpy(p, data, len);
    r->sz_body += len;

    return STATUS_OK;
}

static status_t resp_end_body(struct resp_msg *r)
{
    int ret;
    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%zu", r->sz_body);
    ret = resp_add_header(r, "Content-Length", buf);
    resp_end_header(r);
    return ret;
}

static status_t resp_send(struct resp_msg *r)
{
    HTTP_ERROR_CHECK(write_all(r->sockfd, r->header, r->sz_hdr));
    HTTP_ERROR_CHECK(write_all(r->sockfd, r->body, r->sz_body));
    memset(r->header, '\0', MAX_RESP_HDR_SIZE);
    memset(r->body, '\0', MAX_RESP_BODY_SIZE);
    return STATUS_OK;

esp_error_check_fail:
    return RESP_WRITE_ERR;
}

status_t resp_add_body_vsnprintf(struct resp_msg *r, const char *fmt, ...)
{
    int ret;
    char *p;
    va_list ap;

    p = r->body + r->sz_body;
    if (avail_body(r) <= 0) {
        return RESP_OOM_ERR;
    }
    va_start(ap, fmt);
    ret = vsnprintf(p, avail_body(r), fmt, ap);
    if (ret < 0) {
        return RESP_OOM_ERR;
    }
    va_end(ap);
    r->sz_body += ret;
    return STATUS_OK;
}

static status_t resp_send_error(struct resp_msg *r, int code, const char *text)
{
    r->sz_body = 0;
    r->sz_hdr  = 0;
    resp_start_header(r, code);
    resp_add_header(r, "Content-Type", "text/html");
    resp_add_header(r, "Connection", "close");
    resp_add_body(r, text, strlen(text));
    resp_end_body(r);
    HTTP_ERROR_CHECK(resp_send(r));
    return STATUS_OK;

esp_error_check_fail:
    return RESP_WRITE_ERR;
}

static ssize_t read_all(int fd, char *buf, size_t max_size)
{
    ssize_t ret;
    do {
        ret = read(fd, buf, max_size);
        ESP_LOGI(TAG, "READ:ret:%zu", ret);
    } while (ret == -1 && errno == EINTR);

    if (ret < 0) {
        ESP_LOGE(TAG, "read_all read error, ret:%d, errno:%d, %s\n", (int)ret,
                 errno, strerror(errno));
    }
    return ret;
}

inline static status_t write_all(int sockfd, const char *buf, size_t max_size)
{
    int ret;

    do {
        /* ret = write(fd, buf, max_size); */
        /*
         * MSG_NOSIGNAL (since Linux 2.2)
         *    Don't  generate  a  SIGPIPE  signal if the peer on a
         * stream-oriented socket  has  closed  the  connection.   The  EPIPE
         * error  is  still returned.   This  provides similar behavior to using
         * sigaction(2) to ignore SIGPIPE, but, whereas MSG_NOSIGNAL  is  a
         * per-call  feature, ignoring  SIGPIPE  sets a process attribute that
         * affects all threads in the process.
         */
        ret = send(sockfd, buf, max_size, MSG_NOSIGNAL);
    } while (ret == -1 && errno == EINTR);
    if (ret <= 0) {
        ESP_LOGE(TAG, "write_all error:%d, errno:%d, err:%s", ret, errno,
                 strerror(errno));
        // assert(0);
        return RESP_WRITE_ERR;
    } else {
        return STATUS_OK;
    }
}

static int req_header_parse(int fd, struct req_header *req)
{
    int pret;
    size_t prevbuflen = 0;

    req->nheaders = MAX_HTTP_HEADERS;
    while (1) {
        ssize_t ret;

        ret = read_all(fd, req->buf + req->buf_len,
                       MAX_REQ_HDR_SIZE - req->buf_len);
        if (ret <= 0) {
            ESP_LOGE(TAG, "request read error.");
            return REQ_READ_ERR;
        }

        prevbuflen = req->buf_len;
        req->buf_len += ret;
        pret = phr_parse_request(req->buf, req->buf_len, &req->method,
                                 &req->method_len, &req->path, &req->path_len,
                                 &req->minor_version, req->headers,
                                 &req->nheaders, prevbuflen);
        if (pret > 0) {
            const char *new_path_pos;
            size_t path_len = 0;
            const char *fragment;
            size_t nfragment;

            ESP_LOGI(TAG, "http request parsed., pret:%d, req->buf_len:%zu\n",
                     pret, req->buf_len);
            /* total parsed bytes. */
            req->consumed = pret;
            /* parse path */
            parse_uri_path(req->path, req->path_len, &new_path_pos, &path_len,
                           req->queries, &req->nqueries, &fragment, &nfragment);
            assert(req->path == new_path_pos);
            req->path_len = path_len;
            ESP_LOGI(TAG, "nquries:%zu\n", req->nqueries);
            ESP_LOGI(TAG, "path:%.*s\n", (int)req->path_len, req->path);
            return STATUS_OK;
        } else if (pret == -2) {
            ESP_LOGE(TAG, "http request parse error.");
            return REQ_PARSE_ERR;
        }
        if (req->buf_len == MAX_REQ_HDR_SIZE) {
            ESP_LOGE(TAG, "http request is too long.");
            return REQ_SIZE_ERR;
        }
    }
}

static int handle_http_request(int sockfd)
{
    int ret;
    static struct req_header req;
    struct resp_msg msg;

    memset(&req, 0, sizeof(req));
    memset(&msg, 0, sizeof(msg));
    /* req.sockfd = sockfd; */
    msg.sockfd = sockfd;
    ESP_LOGI(TAG, "before req_header_parse.");
    ret = req_header_parse(sockfd, &req);
    if (ret != STATUS_OK) {
        /* send error */
        resp_send_error(&msg, 404, "File Not Found");
        return ret;
    }
    ESP_LOGI(TAG, "before route request.");
    ret = route_request(&req, sockfd);
    ESP_LOGI(TAG, "end route request. ret:%d", ret);
    /* ret = response_new(sockfd, &req); */
    if (ret != STATUS_OK) {
        resp_send_error(&msg, 404, "File Not Found");
        return ret;
    }
    return ret;
}

static status_t send_chunk_data(struct req_header *req, struct resp_msg *r,
                                int sockfd, const char *data, size_t len)
{
    int nwrite;
    char buf[256];

    if ((data && *data == '\0') || len == 0) {
        HTTP_ERROR_CHECK(write_all(sockfd, "0\r\n\r\n", 5));
        return STATUS_OK;
    }
    nwrite = snprintf(buf, sizeof(buf), "%zx\r\n", len);
    HTTP_ERROR_CHECK(write_all(sockfd, buf, nwrite));
    HTTP_ERROR_CHECK(write_all(sockfd, data, len));
    HTTP_ERROR_CHECK(write_all(sockfd, "\r\n", 2));
    return STATUS_OK;

esp_error_check_fail:
    ESP_LOGI(TAG, "esp_error_check_fail:%s:%d\n", __func__, __LINE__);
    return RESP_WRITE_ERR;
}

status_t do_hello_get(struct req_header *req, int sockfd)
{
    struct resp_msg msg;
    struct resp_msg *r;
    const char *head =
        "<html><head><title> ESP32 HTTP Server</title></head><body><pre>\r\n";

    r = &msg;
    memset(r, 0, sizeof(msg));
    r->sockfd = sockfd;
#if 1
    resp_start_header(r, 200);
    resp_add_header(r, "Content-Type", "text/html");
    resp_add_body(r, head, strlen(head));
    resp_add_body_vsnprintf(r, "request size: %zu\r\n", req->consumed);
    resp_add_body_vsnprintf(r, "method: %.*s\r\n", (int)req->method_len,
                            req->method);
    resp_add_body_vsnprintf(r, "path: %.*s\r\n", (int)req->path_len, req->path);
    resp_add_body_vsnprintf(r, "</pre></body></html>\r\n");
    resp_end_body(r);
    HTTP_ERROR_CHECK(resp_send(r));
#else
    {
        char buf[256];
        resp_start_header(r, 200);
        resp_add_header(r, "Content-Type", "text/html");
        resp_add_header(r, "Connection", "keep-alive");
        resp_add_header(r, "Transfer-Encoding", "chunked");
        resp_end_header(r);
        HTTP_ERROR_CHECK(write_all(sockfd, r->header, r->sz_hdr));

        HTTP_ERROR_CHECK(send_chunk_data(req, r, sockfd, head, strlen(head)));
        HTTP_ERROR_CHECK(send_chunk_data(req, r, sockfd, "CHUNKED reply\r\n",
                                         strlen("CHUNKED reply\r\n")));
        ret = snprintf(buf, 256, "request size: %zu\r\n", req->consumed);
        HTTP_ERROR_CHECK(send_chunk_data(req, r, sockfd, buf, ret));

        ret = snprintf(buf, 256, "method: %.*s\r\n", (int)req->method_len,
                       req->method);
        HTTP_ERROR_CHECK(send_chunk_data(req, r, sockfd, buf, ret));

        ret =
            snprintf(buf, 256, "path: %.*s\r\n", (int)req->path_len, req->path);
        HTTP_ERROR_CHECK(send_chunk_data(req, r, sockfd, buf, ret));

        head = "=====HTTP HEADER=====\r\n";
        ret  = snprintf(buf, 256, "%.*s", (int)strlen(head), head);
        HTTP_ERROR_CHECK(send_chunk_data(req, r, sockfd, head, strlen(head)));
        for (i = 0; i < req->nheaders; i++) {
            struct phr_header *hdr = &req->headers[i];
            HTTP_ERROR_CHECK(
                send_chunk_data(req, r, sockfd, hdr->name, hdr->name_len));
            HTTP_ERROR_CHECK(send_chunk_data(req, r, sockfd, ":", 1));
            HTTP_ERROR_CHECK(
                send_chunk_data(req, r, sockfd, hdr->value, hdr->value_len));
            HTTP_ERROR_CHECK(send_chunk_data(req, r, sockfd, "\r\n", 2));
        }
#if 1
        head = "=====HTTP QUERY=====\r\n";
        ret  = snprintf(buf, 256, "%.*s", (int)strlen(head), head);
        HTTP_ERROR_CHECK(send_chunk_data(req, r, sockfd, head, strlen(head)));
        for (i = 0; i < req->nqueries; i++) {
            struct phr_header *q = &req->queries[i];
            HTTP_ERROR_CHECK(
                send_chunk_data(req, r, sockfd, q->name, q->name_len));
            HTTP_ERROR_CHECK(send_chunk_data(req, r, sockfd, "=", 1));
            HTTP_ERROR_CHECK(
                send_chunk_data(req, r, sockfd, q->value, q->value_len));
            HTTP_ERROR_CHECK(send_chunk_data(req, r, sockfd, "\r\n", 2));
        }
#endif
        ret = snprintf(buf, 256, "</pre></body></html>\r\n");
        HTTP_ERROR_CHECK(send_chunk_data(req, r, sockfd, buf, ret));
        /* write_all(sockfd, "0\r\n\r\n", 5); */
        HTTP_ERROR_CHECK(send_chunk_data(req, r, sockfd, "", 0));
    }

#endif
    return STATUS_OK;

esp_error_check_fail:
    return RESP_WRITE_ERR;
}

/* @route: "/r/hello" {do_hello} */
status_t do_hello(struct req_header *req, int sockfd)
{
    enum http_method method;

    method = get_method(req->method, req->method_len);
    ESP_LOGI(TAG, "method:%d\n", (int)method);
    return do_hello_get(req, sockfd);
}

/* @route: "/index.html" | "/index.htm" | "/index" | "/" {do_homepage} */
status_t do_homepage(struct req_header *req, int sockfd)
{
    struct resp_msg r = {
        .sockfd = sockfd,
    };

    resp_send_error(&r, 200, INDEX_HTML);
    return STATUS_OK;
}

static const char *mime_type(struct req_header *req)
{
    int count = 0;
    size_t len;
    const char *end  = req->path + req->path_len;
    const char *mime = "";

    while (count++ < req->path_len) {
        if (*end == '.' && count > 0) {
            end++;
            len = req->path_len - (end - req->path);
            ESP_LOGI(TAG, "*end[%c%c%c%c], len:%zu\n", end[0], end[1], end[2],
                     end[3], len);
            mime = get_mime_type(end, len);
        }
        end--;
    }
    return mime == NULL ? "text/plain" : mime;
}

static status_t remap_path(const struct req_header *req, char *path, size_t len)
{
#ifdef LINUX_BUILD
    const char prefix[] = "/home/dpu/esp/esp-idf/http_server";
    char *p             = path + sizeof(prefix) - 1; /* strlen(prefix); */
    if (path == NULL || sizeof(prefix) + req->path_len >= len) {
        ESP_LOGE(TAG, "path prefix is too long");
        return RESP_OOM_ERR;
    }
    memcpy(path, prefix, sizeof(prefix) - 1);
    memcpy(p, req->path, req->path_len);
    p[req->path_len] = '\0';
#else
    const char *p = req->path + 8; /* substract strlen("/static/") */
    snprintf(path, len, "/spiffs/%llx", djb2_hash(p, req->path_len - 8));
#endif
    return STATUS_OK;
}

/* Ragel: "/static" */
status_t do_file(struct req_header *req, int sockfd)
{
    enum http_method method;
    int fd = -1;
    struct resp_msg r;

    memset(&r, 0, sizeof(r));
    method = get_method(req->method, req->method_len);
    ESP_LOGI(TAG, "method:%d\n", (int)method);
    if (method == HTTP_GET) {
        int is_gzip = -1; /* -1: unchecked, 0: not a gzip, 1: gzip */
        const char *mime;
        size_t cnt = 0;
        char path[512];

        remap_path(req, path, sizeof(path));

        mime = mime_type(req);

        ESP_LOGI(TAG, "mime:%.*s, strlen(mime):%zu\n", 6, mime, strlen(mime));
        ESP_LOGI(TAG, "path:%s\n", path);
        // ESP_LOGE(TAG, "PATH:%.*s", strlen(path), path);
        fd = open(path, O_RDONLY);
        if (fd < 0) {
            ESP_LOGE(TAG, "failed to open file:%s\n", path);
            goto esp_error_check_fail;
        }
        resp_start_header(&r, 200);
        resp_add_header(&r, "Content-Type", mime);
        /* 24hrs */
        resp_add_header(&r, "Cache-Control", "public, max-age=31536000");

        resp_add_header(&r, "Connection", "keep-alive");
        resp_add_header(&r, "Transfer-Encoding", "chunked");
        /*
        resp_end_header(&r);
        HTTP_ERROR_CHECK(write_all(sockfd, r.header, r.sz_hdr));
        */

        do {
            ssize_t ret;
            char buf[MAX_RESP_CHUNK_SIZE];
            ret = read_all(fd, buf, sizeof(buf));
            ESP_LOGI(TAG, "READ SIZE:%zu\n", ret);
            if (ret > 0) {
                if (is_gzip < 0) {
                    is_gzip = (buf[0] == 0x1f && buf[1] == 0x8b) ? 1 : 0;
                    if (is_gzip) {
                        resp_add_header(&r, "Content-Encoding", "gzip");
                    }
                    resp_end_header(&r);
                    HTTP_ERROR_CHECK(write_all(sockfd, r.header, r.sz_hdr));
                }
                HTTP_ERROR_CHECK(send_chunk_data(req, &r, sockfd, buf, ret));
                cnt += ret;
            } else if (ret == 0) {
                ESP_LOGI(TAG, "read file EOF");
                break;
            } else {
                ESP_LOGE(TAG, "errno:%d\n", errno);
            }

        } while (true);
        ESP_LOGI(TAG, "total read bytes:%zu\n", cnt);
        HTTP_ERROR_CHECK(send_chunk_data(req, &r, sockfd, "", 0));
        close(fd);
        fd = -1;
    }
    return STATUS_OK;
esp_error_check_fail:
    if (fd > 0) {
        close(fd);
    }
    return RESP_WRITE_ERR;
}

//////////////////////////////////////////////////////////////////

void httpd_task(void *p)
{
    int ret;
    int optval = 1;

    int sockfd, new_sockfd;
    socklen_t addr_len;
    struct sockaddr_in sock_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        ESP_LOGI(TAG, "failed");
        goto failed2;
    }
    ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "HTTP server socket bind ......");
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family      = AF_INET;
    sock_addr.sin_addr.s_addr = 0;
    sock_addr.sin_port        = htons(SERVER_PORT);
    ret = bind(sockfd, (struct sockaddr *)&sock_addr, sizeof(sock_addr));
    if (ret) {
        ESP_LOGI(TAG, "failed");
        goto failed3;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "HTTP server socket listen ......");
    ret = listen(sockfd, 2);
    if (ret) {
        ESP_LOGI(TAG, "failed");
        goto failed3;
    }
    ESP_LOGI(TAG, "OK");

reconnect:
    ESP_LOGI(TAG, "HTTP server socket accept client ......");
    new_sockfd = accept(sockfd, (struct sockaddr *)&sock_addr, &addr_len);
    if (new_sockfd < 0) {
        ESP_LOGI(TAG, "failed");
        goto failed4;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "HTTP server read message ......");
    handle_http_request(new_sockfd);
    close(new_sockfd);
    new_sockfd = -1;
failed4:
    goto reconnect;
failed3:
    close(sockfd);
    sockfd = -1;
failed2:
    vTaskDelete(NULL);
    return;
}

void http_server_init(void)
{
    int ret;
    xTaskHandle httpd_handle;
    esp_log_level_set(TAG, ESP_LOG_ERROR);
    ret = xTaskCreate(httpd_task, TASK_NAME, TASK_STACK_WORDS, NULL,
                      TASK_PRIORITY, &httpd_handle);

    if (ret != pdPASS) {
        ESP_LOGI(TAG, "create task %s failed", TASK_NAME);
    }
}
