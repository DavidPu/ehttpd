#include <string.h>
#include <stdio.h>
#include "http_server.h"

/* match exact path instead of prefix matching:
 * 'GET /hello HTTP 1.0'  => accepted.
 * 'GET /helloworld HTTP 1.0' => rejected.
 */
#define CALL_IF_EXACT_MATCH(func)                        \
    do {                                                 \
        const char n = *(p+1);                           \
        if (n == ' ' || n == '?' || n == '#') {          \
            return func(req, sockfd);                    \
        } else {                                         \
            return REQ_ROUTE_ERR;                        \
        }                                                \
    } while (0)

%%{
    machine url_route;
    main := |*

    "/index.html" | "/index.htm" | "/index" | "/" { CALL_IF_EXACT_MATCH(do_homepage); };
    "/r/hello"                                    { CALL_IF_EXACT_MATCH(do_hello); };
    "/r/" lower{3,5} "/" (digit)+                 { CALL_IF_EXACT_MATCH(do_hello); };
    "/static/"                                    { return do_file(req, sockfd); };

    *|;
}%%

%% write data nofinal;

status_t route_request(struct req_header *req, int sockfd)
{
    const char *p = req->path;
    const char *pe = p + req->path_len + 1; /* include the whitespace after match. */
    const char *eof = pe;
    const char *ts;
    const char *te;
    int cs, act;
    
    /* suppress -Wunused-const-variable warnings */
    (void)url_route_error;
    (void)url_route_en_main;
    (void)ts;

    /********** write init **************/
    %% write init;
    /********** write exec **************/
    %% write exec;
    return REQ_ROUTE_ERR;
}
