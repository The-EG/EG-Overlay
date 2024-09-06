#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "dbg-sink.h"
#include "../utils.h"

struct log_dbg_sink_t {
    log_sink_t sink;
};

void log_dbg_sink_write(log_sink_t *sink, enum LOGGER_LEVEL level, const char *message);
log_sink_t *log_dbg_sink_clone(log_sink_t *sink);

log_sink_t *log_dbg_sink_new() {
    struct log_dbg_sink_t *new = calloc(1, sizeof(struct log_dbg_sink_t));

    new->sink.write = &log_dbg_sink_write;
    new->sink.clone = &log_dbg_sink_clone;

    return (log_sink_t *)new;
}

void log_dbg_sink_write(log_sink_t *sink, enum LOGGER_LEVEL level, const char *message) {
    UNUSED_PARAM(sink);
    UNUSED_PARAM(level);
    
    size_t msglen = strlen(message);
    char *msgnl = calloc(msglen+2, sizeof(char));
    memcpy(msgnl, message, msglen);

    msgnl[msglen] = '\n';

    OutputDebugString(msgnl);
    free(msgnl);
}

log_sink_t *log_dbg_sink_clone(log_sink_t *sink) {
    UNUSED_PARAM(sink);
    return log_dbg_sink_new();
}

