#include <stdlib.h>
#include <stdio.h>
#include <share.h>
#include "file-sink.h"
#include "logger.h"
#include "../utils.h"

typedef struct log_file_sink_t {
    log_sink_t sink;

    FILE *f;
    int close_file;
} log_file_sink_t;

void log_file_sink_free(log_file_sink_t *sink) {
    if (sink->close_file) fclose(sink->f);
    egoverlay_free(sink);
}

void log_file_sink_write(log_file_sink_t *sink, enum LOGGER_LEVEL level, const char *message) {
    (void)level;
    
    fprintf(sink->f, "%s\n", message);

    fflush(sink->f);
}

log_sink_t *log_file_sink_clone(log_file_sink_t *sink) {
    log_file_sink_t *clone = egoverlay_calloc(1, sizeof(log_file_sink_t));
    clone->sink.clone = (log_sink_clone_fn*)&log_file_sink_clone;
    clone->sink.write = (log_sink_write_fn*)&log_file_sink_write;
    clone->sink.free = (log_sink_free_fn*)&log_file_sink_free;
    clone->close_file = 0;
    clone->f = sink->f;

    return (log_sink_t*)clone;
}

log_sink_t *log_file_sink_new(const char *path) {
    log_file_sink_t *sink = egoverlay_calloc(1, sizeof(log_file_sink_t));

    sink->sink.free = (log_sink_free_fn*)&log_file_sink_free;
    sink->sink.clone = (log_sink_clone_fn*)&log_file_sink_clone;
    sink->sink.write = (log_sink_write_fn*)&log_file_sink_write;

    sink->close_file = 1;
    //sink->f = fopen(path, "wt");
    sink->f = _fsopen(path, "wb", _SH_DENYWR);

    if (!sink->f) {
        egoverlay_free(sink);
        return NULL;
    }

    return (log_sink_t*)sink;
}
