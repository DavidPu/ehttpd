/* HTTP Server

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "http_server.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "picohttpparser.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"

#include "nvs_flash.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"


const static char *TAG = "httpd";
#define HTTP_EXAMPLE_TASK_NAME        "httpd"
#define HTTP_EXAMPLE_TASK_STACK_WORDS 1024 * 10
#define HTTP_EXAMPLE_TASK_PRIORITY    8

#define HTTP_EXAMPLE_LOCAL_TCP_PORT     80

#define CONFIG_HTTP_RECV_BUF_LEN 1024
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

    len += snprintf(send_data + len, total - len, "request size: %u bytes\r\n", req_len);
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

    conlen = snprintf(cont_length, 256, "Content-Length: %u\r\n\r\n", len);
    write(sockfd, cont_length, conlen);
    ESP_LOGI(TAG, "%s||||\n", cont_length);
    ESP_LOGI(TAG, "len:%u, strlen:%u\n", len, strlen(send_data));
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

static void httpd_task(void *p)
{
    int ret;

    int sockfd, new_sockfd;
    socklen_t addr_len;
    struct sockaddr_in sock_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        ESP_LOGI(TAG, "failed");
        goto failed2;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "HTTP server socket bind ......");
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = 0;
    sock_addr.sin_port = htons(HTTP_EXAMPLE_LOCAL_TCP_PORT);
    ret = bind(sockfd, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
    if (ret) {
        ESP_LOGI(TAG, "failed");
        goto failed3;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "HTTP server socket listen ......");
    ret = listen(sockfd, 32);
    if (ret) {
        ESP_LOGI(TAG, "failed");
        goto failed3;
    }
    ESP_LOGI(TAG, "OK");

reconnect:
    ESP_LOGI(TAG, "HTTP server socket accept client ......");
    new_sockfd = accept(sockfd, (struct sockaddr *)&sock_addr, &addr_len);
    if (new_sockfd < 0) {
        ESP_LOGI(TAG, "failed" );
        goto failed4;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "HTTP server read message ......");
    do {
        handle_http_request(new_sockfd);
    } while (1);
    
    close(new_sockfd);
    new_sockfd = -1;
failed4:
    goto reconnect;
failed3:
    close(sockfd);
    sockfd = -1;
failed2:
    vTaskDelete(NULL);
    return ;
} 

void http_server_init(void)
{
    int ret;
    xTaskHandle httpd_handle;

    ret = xTaskCreate(httpd_task,
                      HTTP_EXAMPLE_TASK_NAME,
                      HTTP_EXAMPLE_TASK_STACK_WORDS,
                      NULL,
                      HTTP_EXAMPLE_TASK_PRIORITY,
                      &httpd_handle); 

    if (ret != pdPASS)  {
        ESP_LOGI(TAG, "create task %s failed", HTTP_EXAMPLE_TASK_NAME);
    }
}
