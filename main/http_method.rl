#include <string.h>
#include <stdio.h>
#include "http_server.h"

%%{
    machine http_method;
    main := |*

    "DELETE" { return HTTP_DELETE; };
    "GET"    { return HTTP_GET;    };
    "HEAD"   { return HTTP_HEAD;   };
    "POST"   { return HTTP_POST;   };
    "PUT"    { return HTTP_PUT;    };

    *|;
}%%

%% write data nofinal;

enum http_method get_method(const char *method, size_t method_len)
{
    const char *p = method;
    const char *pe = p + method_len;
    const char *ts;
    const char *te;
    int cs;
    int act;
    
    /* suppress -Wunused-const-variable warnings */
    (void)http_method_error;
    (void)http_method_en_main;
    (void)act;
    (void)te;
    (void)ts;

    /********** write init **************/
    %% write init;
    /********** write exec **************/
    %% write exec;
    return -1;
}
