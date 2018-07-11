#include <stddef.h>
#include "http_server.h"

%%{
  machine uripath_parser;
  scary    = ( cntrl | 127 | ' ' | '"' | '#' | '<' | '>' ) ;
  pathchar = any -- ( scary | '?' | '#' ) ;
  quervalchar = any -- ( scary | '#' | '&') ;
  querkeychar = quervalchar -- '=' ;
  fragchar = any -- ( scary ) ;

  action querykey_start {
    if (*num_params >= max_params) {
      err = RESP_OOM_ERR;
      fbreak;
    }
    params[*num_params].name = p;
    params[*num_params].value = NULL;
  }

  action querykey_end {
    params[*num_params].name_len = p - params[*num_params].name;
  }

  action queryvalue_start {
    params[*num_params].value = p;
  }

  action queryvalue_end {
    params[*num_params].value_len = p - params[*num_params].value;
  }

  action queryentry_end {
    (*num_params)++;
  }

  action frag_start {
    *fragment = p;
  }

  action frag_end {
    *fragment_len = p - *fragment;
  }

  action path_start {
    *path = p;
  }

  action path_end {
    *path_len = p - *path;
  }

  queryentry = querkeychar+ >querykey_start %querykey_end ('=' quervalchar* >queryvalue_start %queryvalue_end)? %queryentry_end;
  query = '?' queryentry? ('&' queryentry)*;
  fragment = '#' (fragchar* >frag_start %frag_end);
  path = ('/' pathchar*) >path_start %path_end;
  full_path = path query? fragment?;

  main := full_path;

  write data;
}%%

status_t parse_uri_path(const char *buf_start, size_t buf_len,
                   const char **path, size_t *path_len,
                   struct phr_header *params, size_t *num_params,
                   const char **fragment, size_t *fragment_len)
{
  const char *p = buf_start;
  const char *pe = p + buf_len;
  int cs;
  const char *eof = pe;
  int err = 0;

  *path = NULL;
  *path_len = 0;
  size_t max_params = MAX_HTTP_HEADERS;
  *num_params = 0;
  *fragment = NULL;
  *fragment_len = 0;

  (void) uripath_parser_error;
  (void) uripath_parser_en_main;

  %%{
    write init;
    write exec;
  }%%

  if (err) {
    return err;
  } else if (cs < uripath_parser_first_final) {
    return REQ_PARSE_ERR;
  }

  return STATUS_OK;
}
