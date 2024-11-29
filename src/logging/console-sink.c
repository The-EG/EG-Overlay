#include "console-sink.h"
#include "../utils.h"
#include <windows.h>

struct log_console_sink_s {
    log_sink_t sink;
    HANDLE consoleout;
};

void log_console_sink_write(log_sink_t *sink, enum LOGGER_LEVEL level, const char *message);
log_sink_t *log_console_sink_clone(log_sink_t *sink);

log_sink_t *log_console_sink_new() {
    struct log_console_sink_s *new = egoverlay_calloc(1, sizeof(struct log_console_sink_s));

    new->sink.write = &log_console_sink_write;
    new->sink.clone = &log_console_sink_clone;

    new->consoleout = GetStdHandle(STD_ERROR_HANDLE);

    return (log_sink_t*)new;
}

void log_console_sink_write(log_sink_t *sink, enum LOGGER_LEVEL level, const char *message) {
    UNUSED_PARAM(level);
    struct log_console_sink_s *con = (struct log_console_sink_s*)sink;

    char *line = egoverlay_calloc(strlen(message)+2, sizeof(char));
    memcpy(line, message, strlen(message));
    line[strlen(message)] = '\n';

    WriteFile(con->consoleout, line, (DWORD)strlen(message) + 1, NULL, NULL);
    egoverlay_free(line);
}

log_sink_t *log_console_sink_clone (log_sink_t *sink) {
    UNUSED_PARAM(sink);
    return log_console_sink_new();
}
