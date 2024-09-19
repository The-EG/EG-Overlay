#pragma once

typedef struct web_request_t web_request_t;

void web_request_init();
void web_request_cleanup();

web_request_t *web_request_new(const char *url);
void web_request_free(web_request_t *request);

typedef void web_request_callback(int code, char *data, web_request_t *request);

void web_request_queue(
    web_request_t *request,
    web_request_callback *callback,
    int free_after,
    const char *source,
    int
);

void web_request_add_header(web_request_t *request, const char *name, const char* value);
void web_request_add_query_parameter(web_request_t *request, const char *name, const char *value);
