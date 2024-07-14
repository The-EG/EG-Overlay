#include "helpers.h"
#include "stderr-sink.h"

logger_t *stderr_logger_new(const char *name, enum LOGGER_LEVEL level) {
    logger_t *log = logger_new(name);
    logger_set_level(log, level);
    log_sink_t *sink = log_stderr_sink_new();
    logger_add_sink(log, sink);

    return log;
}