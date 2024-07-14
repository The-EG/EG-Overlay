#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "stderr-sink.h"
#include "../utils.h"

struct log_stderr_sink_s {
    log_sink_t sink;
};

void log_stderr_sink_write(log_sink_t *sink, enum LOGGER_LEVEL level, const char *message);
log_sink_t *log_stderr_sink_clone(log_sink_t *sink);


log_sink_t *log_stderr_sink_new() {
    struct log_stderr_sink_s *new = calloc(1, sizeof(struct log_stderr_sink_s));

    new->sink.write = &log_stderr_sink_write;
    new->sink.clone = &log_stderr_sink_clone;

    return (log_sink_t *)new;
}


void log_stderr_sink_write(log_sink_t *sink, enum LOGGER_LEVEL level, const char *message) {
    UNUSED_PARAM(sink);
    UNUSED_PARAM(level);
    fprintf(stderr, "%s\n", message);
}

log_sink_t *log_stderr_sink_clone(log_sink_t *sink) {
    UNUSED_PARAM(sink);
    return log_stderr_sink_new();
}