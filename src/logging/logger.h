#pragma once
#include "sink.h"

typedef struct logger_t logger_t;

enum LOGGER_LEVEL {
    LOGGER_LEVEL_NONE,
    LOGGER_LEVEL_ERROR,
    LOGGER_LEVEL_WARNING,
    LOGGER_LEVEL_INFO,
    LOGGER_LEVEL_DEBUG
} ;

#define DEFAULT_LOGGER_LEVEL LOGGER_LEVEL_DEBUG

void logger_init();
void logger_cleanup();

logger_t *logger_new(const char *name);
void logger_free(logger_t *log);

void logger_set_default(logger_t *logger);
logger_t *logger_get(const char *name);

void logger_set_level(logger_t *log, enum LOGGER_LEVEL level);

void logger_log(logger_t *log, enum LOGGER_LEVEL level, const char *message, ...);
#define logger_error(log, ...) logger_log(log, LOGGER_LEVEL_ERROR, __VA_ARGS__)
#define logger_warn(log, ...) logger_log(log, LOGGER_LEVEL_WARNING, __VA_ARGS__)
#define logger_info(log, ...) logger_log(log, LOGGER_LEVEL_INFO, __VA_ARGS__)
#define logger_debug(log, ...) logger_log(log, LOGGER_LEVEL_DEBUG, __VA_ARGS__)

void logger_add_sink(logger_t *log, log_sink_t *sink);

const char *logger_level_to_str(enum LOGGER_LEVEL level);
enum LOGGER_LEVEL logger_str_to_level(const char *level);
