#include <stdlib.h>
#include <assert.h>
#include "sink.h"

void log_sink_free(log_sink_t *sink) {
    if (sink->free) sink->free(sink);
    else free(sink);
}

void log_sink_write(log_sink_t *sink, enum LOGGER_LEVEL level, const char *message) {
    sink->write(sink, level, message);
}

log_sink_t *log_sink_clone(log_sink_t *sink) {
    return sink->clone(sink);
}