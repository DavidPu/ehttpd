#ifndef _HTTP_SERVER_H_
#define _HTTP_SERVER_H_

#include <stdint.h>
#include "picohttpparser.h"

#ifndef CONFIG_MAX_REQ_HDR_SIZE
#define CONFIG_MAX_REQ_HDR_SIZE 4096
#endif

#ifndef CONFIG_SERVER_PORT
#define CONFIG_SERVER_PORT 80
#endif

#define MAX_HTTP_HEADERS 32
#define SERVER_PORT CONFIG_SERVER_PORT
#define MAX_REQ_HDR_SIZE CONFIG_MAX_REQ_HDR_SIZE
#define MAX_RESP_HDR_SIZE 256
#define MAX_RESP_BODY_SIZE 256
#define MAX_RESP_CHUNK_SIZE 4096

#define HTTP_ERROR_CHECK(x)            \
    do {                               \
        status_t __err_rc = (x);       \
        if (__err_rc != STATUS_OK) {   \
            goto esp_error_check_fail; \
        }                              \
    } while (0)

enum {

    STATUS_OK,     /* request parsed correctly */
    REQ_PARSE_ERR, /* request parse error */
    REQ_READ_ERR,  /* socket read error */
    REQ_SIZE_ERR,  /* request large than MAX_REQ_HDR_SIZE */
    REQ_ROUTE_ERR, /* no proper handler or the request. */
    RESP_OOM_ERR,
    RESP_WRITE_ERR,
};

typedef int32_t status_t;

struct req_header {
    const char *method; /* pointer point to address in buf[] */
    const char *path;   /* pointer point to address in buf[] */
    size_t method_len;
    size_t path_len;
    size_t nheaders;
    size_t nqueries;
    int minor_version;
    size_t buf_len;
    size_t consumed; /* total consumed buffer size since last read(2). */
    struct phr_header headers[MAX_HTTP_HEADERS];
    struct phr_header queries[MAX_HTTP_HEADERS];
    char buf[MAX_REQ_HDR_SIZE];
};

/* supported http request methods. */
#define HTTP_METHOD_MAP(XX) \
    XX(0, DELETE, DELETE)   \
    XX(1, GET, GET)         \
    XX(2, HEAD, HEAD)       \
    XX(3, POST, POST)       \
    XX(4, PUT, PUT)

enum http_method {
#define XX(num, name, string) HTTP_##name = num,
    HTTP_METHOD_MAP(XX)
#undef XX
};

enum http_method get_method(const char *method, size_t method_len);
const char *get_mime_type(const char *suffix, size_t length);
status_t parse_uri_path(const char *buf_start, size_t buf_len,
                        const char **path, size_t *path_len,
                        struct phr_header *params, size_t *num_params,
                        const char **fragment, size_t *fragment_len);

/*
 * route request to proper do_xxx handler.
 * generated from urlroute.rl.
 */
status_t route_request(struct req_header *req, int sockfd);

/* Ragel: "/r/hello" */
status_t do_hello(struct req_header *req, int sockfd);

/* Ragel: "/static" */
status_t do_file(struct req_header *req, int sockfd);

/* @route: "/index.html" | "/index.htm" | "/index" | "/" {do_homepage} */
status_t do_homepage(struct req_header *req, int sockfd);

void http_server_init(void);
void httpd_task(void *p);

#endif
