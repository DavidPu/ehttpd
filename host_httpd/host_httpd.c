
// http://www.codingpedia.org/ama/how-to-test-a-rest-api-from-command-line-with-curl
// https://docs.oracle.com/cloud/latest/marketingcs_gs/OMCAB/Developers/GettingStarted/API%20requests/curl-requests.htm

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "picohttpparser.h"
#include "debug.h"

#define TAG "httpd"
#define CONFIG_HTTP_PORT 8888
#define CONFIG_HTTP_RECV_BUF_LEN 4096
#define MAX_BACKLOG 32
#define MAX_HTTP_HEADERS 32

#define HTTP_EXAMPLE_SERVER_ACK "HTTP/1.1 200 OK\r\n" \
                                "Content-Type: text/html\r\n" \
                                "Content-Length: 98\r\n\r\n" \
                                "<html>\r\n" \
                                "<head>\r\n" \
                                "<title>HTTP example</title></head><body>\r\n" \
                                "HTTP server example!\r\n" \
                                "</body>\r\n" \
                                "</html>\r\n" \
                                "\r\n"

#define HTTP_OK "HTTP/1.1 200 OK\r\n"
#define CONT_HTML "Content-Type: text/html\r\n"

static int response(int sockfd, const char *method, size_t method_len,
                    const char *path, size_t path_len, struct phr_header *headers,
                    size_t num_headers, size_t req_len)
{
    char send_data[CONFIG_HTTP_RECV_BUF_LEN] = {'\0'};
    const char http_ok[] = HTTP_OK;
    const char cont_html[] = CONT_HTML;
    size_t total = CONFIG_HTTP_RECV_BUF_LEN;
    size_t len = 0;
    char cont_length[256] = {'\0'};
    size_t conlen;
    int i;

    write(sockfd, http_ok, sizeof(http_ok));
    write(sockfd, cont_html, sizeof(cont_html));

    len += snprintf(send_data + len, total - len, "request size: %lu bytes\r\n", req_len);
    len += snprintf(send_data + len, total - len, "method: %.*s\r\n", (int)method_len, method);
    len += snprintf(send_data + len, total - len, "path: %.*s\r\n", (int)path_len, path);
    for (i = 0; i < num_headers; i++) {
        len += snprintf(send_data + len, total - len, "%.*s: %.*s\r\n",
                        (int)headers[i].name_len,
                        headers[i].name,
                        (int)headers[i].value_len,
                        headers[i].value);
    }
    len += snprintf(send_data + len, total - len, "\r\n");

    conlen = snprintf(cont_length, 256, "Content-Length: %lu\r\n\r\n", len);
    write(sockfd, cont_length, conlen);
    ESP_LOGI(TAG, "%s||||\n", cont_length);
    ESP_LOGI(TAG, "len:%lu, strlen:%lu\n", len, strlen(send_data));
    ESP_LOGI(TAG, "data:%.*s\n", (int)len, send_data);
    write(sockfd, send_data, len);

    return 0;
}

static int handle_http_request(int sockfd)
{
    char buf[CONFIG_HTTP_RECV_BUF_LEN] = {0};
    struct phr_header headers[MAX_HTTP_HEADERS] = {0};
    const char *method;
    const char *path;
    size_t buflen = 0;
    size_t prevbuflen = 0;
    size_t method_len = 0;
    size_t path_len = 0;
    size_t num_headers = MAX_HTTP_HEADERS;
    int pret;
    int minor_version;

    while (1) {
        ssize_t rret;
        do {
            rret = read(sockfd, buf + buflen, sizeof(buf) - buflen);
        } while (rret == -1 && errno == EINTR);
        if (rret <= 0) {
            ESP_LOGE(TAG, "request read error.\n");
            return -3;
        }
        prevbuflen = buflen;
        buflen += rret;
        pret = phr_parse_request(buf, buflen, &method, &method_len,
                                        &path, &path_len, &minor_version,
                                        headers, &num_headers, prevbuflen);
        if (pret > 0) {
            ESP_LOGI(TAG, "http request parsed.\n");
            break;
        } else if (pret == -2) {
            ESP_LOGI(TAG, "http request parse error.\n");
            return -1;
        }
        if (buflen == sizeof(buf)) {
            ESP_LOGI(TAG, "http request is too long.\n");
            return -2;
        }
    }
    response(sockfd, method, method_len, path, path_len, headers, num_headers, pret);
    return 0;
}

int main(int argc, char *argv[])
{
    struct sockaddr_in sock_addr;
    socklen_t addrlen;
    int ret;
    int sockfd;
    int new_sockfd;
    int optval = 1;

    ESP_LOGE(TAG, "http://0.0.0.0:%d\n", CONFIG_HTTP_PORT);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        ESP_LOGE(TAG, "failed to create sockfd.\n");
        goto failed;
    }
    ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    if (ret) {
        ESP_LOGE(TAG, "setsockopt failed.\n");
        goto failed;
    }
    ESP_LOGE(TAG, "OK\n");
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = 0;
    sock_addr.sin_port = htons(CONFIG_HTTP_PORT);
    ret = bind(sockfd, (struct sockaddr *)&sock_addr, sizeof(sock_addr));
    if (ret) {
        ESP_LOGE(TAG, "bind failed\n");
        goto bind_fail;
    }

    ESP_LOGI(TAG, "HTTP server socket listening ...\n");
    ret = listen(sockfd, MAX_BACKLOG);
    if (ret) {
        ESP_LOGE(TAG, "listen failed.\n");
        goto bind_fail;
    }

    do {
        ESP_LOGI(TAG, "HTTP server socket accept client ...\n");
        new_sockfd = accept(sockfd, (struct sockaddr *)&sock_addr, &addrlen);
        if (new_sockfd < 0) {
            ESP_LOGE(TAG, "accept failed.\n");
            goto bind_fail;
        }

        ESP_LOGI(TAG, "HTTP server read message ...\n");
        handle_http_request(new_sockfd);
        close(new_sockfd);
        new_sockfd = -1;
    } while (1);
bind_fail:
    close(sockfd);
    sockfd = -1;
failed:
    return -1;
}
