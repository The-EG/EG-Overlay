#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "event-sink.h"
#include "logger.h"
#include "../lua-manager.h"
#include <jansson.h>

typedef struct log_event_sink_t {
    log_sink_t sink;
} log_event_sink_t;

void log_event_sink_write(log_sink_t *sink,  enum LOGGER_LEVEL level, const char *message);
log_sink_t *log_event_sink_clone(log_sink_t *sink);


log_sink_t *log_event_sink_new() {
    log_event_sink_t *new = calloc(1, sizeof(log_event_sink_t));

    new->sink.write = &log_event_sink_write;
    new->sink.clone = &log_event_sink_clone;

    return (log_sink_t *)new;
}


void log_event_sink_write(log_sink_t *sink,  enum LOGGER_LEVEL level, const char *message) {
    (void)sink; // no warning
    json_t *data = json_object();
    json_t *msg = json_string(message);
    json_t *lvl = json_string(logger_level_to_str(level));
    json_object_set(data, "message", msg);
    json_object_set(data, "level", lvl);

    lua_manager_queue_event("log-message", data);
    
    json_decref(lvl);
    json_decref(msg);
    json_decref(data);
}

log_sink_t *log_event_sink_clone(log_sink_t *sink) {
    (void)sink; // no warning
    return log_event_sink_new();
}