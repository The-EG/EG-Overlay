#pragma once
enum LOGGER_LEVEL;

typedef struct log_sink_t log_sink_t;

typedef void log_sink_free_fn(log_sink_t *sink);
typedef void log_sink_write_fn(log_sink_t *sink, enum LOGGER_LEVEL level, const char *message);
typedef log_sink_t *log_sink_clone_fn(log_sink_t *sink);

struct log_sink_t {
    log_sink_free_fn *free;
    log_sink_write_fn *write;
    log_sink_clone_fn *clone;
};

void log_sink_free(log_sink_t *sink);
void log_sink_write(log_sink_t *sink, enum LOGGER_LEVEL level,  const char *message);
log_sink_t *log_sink_clone(log_sink_t *sink);