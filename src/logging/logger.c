#include <time.h>
#include <sys/timeb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "logger.h"
#include "../utils.h"

struct sink_list_node {
    log_sink_t *sink;
    struct sink_list_node *next;
};

struct logger_t {
    enum LOGGER_LEVEL level;
    struct sink_list_node *sinks;
    const char *name;
};

#define LOGGER_HASHMAP_SIZE 250
struct logging_s {
    char **hash_map;
    logger_t **loggers;
    logger_t *default_logger;
};

struct logging_s *logging = NULL;

void logger_init() {
    logging = egoverlay_calloc(1, sizeof(struct logging_s));
    logging->hash_map = egoverlay_calloc(LOGGER_HASHMAP_SIZE, sizeof(char *));
    logging->loggers = egoverlay_calloc(LOGGER_HASHMAP_SIZE, sizeof(logger_t *));
}

void logger_cleanup() {
    for (size_t h=0;h<LOGGER_HASHMAP_SIZE;h++) {
        if (logging->hash_map[h]) {
            logger_free(logging->loggers[h]);
            egoverlay_free(logging->hash_map[h]);
        }
    }

    egoverlay_free(logging->hash_map);
    egoverlay_free(logging->loggers);
    egoverlay_free(logging);
}

void logger_set_default(logger_t *logger) {
    logging->default_logger = logger;
}

logger_t *logger_get(const char *name) {
    uint32_t hash = djb2_hash_string(name);
    uint32_t hash_ind = hash % LOGGER_HASHMAP_SIZE;

    // look for the name in the hash map, starting with an exact map.
    // in an event of a collision, the next open space will be used, wrapping
    // around to the beginning if the end is reached while looking
    while (1) {
        if (logging->hash_map[hash_ind]==NULL) break; // this logger doesn't exist yet

        if (logging->hash_map[hash_ind] && strcmp(logging->hash_map[hash_ind], name)==0) {
            return logging->loggers[hash_ind]; // this is the logger, return it
        }
        hash_ind++; // hash collision, linear probe

        if (hash_ind==hash %LOGGER_HASHMAP_SIZE) {
            // we should never get here
            fprintf(stderr, "logger map overflow.");
            abort();
        }
    }

    // at this point we know the logger doesn't exist yet, so create it from
    // the default logger
    logger_t *new_logger = egoverlay_calloc(1, sizeof(logger_t));
    new_logger->level = logging->default_logger->level;

    struct sink_list_node *s = logging->default_logger->sinks;
    while (s) {
        log_sink_t *sink = log_sink_clone(s->sink);
        logger_add_sink(new_logger, sink);
        s = s->next;
    }

    // now figure out where to put it in the hash map
    hash_ind = hash % LOGGER_HASHMAP_SIZE;
    while (1) {
        if (logging->hash_map[hash_ind]==NULL) {
            logging->hash_map[hash_ind] = egoverlay_calloc(strlen(name)+1, sizeof(char));
            memcpy(logging->hash_map[hash_ind], name, strlen(name));
            new_logger->name = logging->hash_map[hash_ind];
            logging->loggers[hash_ind] = new_logger;

            return new_logger;
        }
        hash_ind++;
        if (hash_ind == hash % LOGGER_HASHMAP_SIZE) {
            fprintf(stderr, "logger map overflow.");
            abort();
        }
    }

    // should never reach here
    return NULL;
}

logger_t *logger_new(const char *name) {
    logger_t *new = egoverlay_calloc(1, sizeof(logger_t));

    new->level = LOGGER_LEVEL_INFO;
    new->name = name;

    return new;
}

void logger_free(logger_t *log) {
    struct sink_list_node *s = log->sinks;

    while (s) {
        log_sink_free(s->sink);
        struct sink_list_node *prev_s = s;
        s = s->next;
        egoverlay_free(prev_s);
    }

    egoverlay_free(log);
}

void logger_set_level(logger_t *log, enum LOGGER_LEVEL level) {
    log->level = level;
}

const char *logger_level_to_str(enum LOGGER_LEVEL level) {
    switch(level) {
    case LOGGER_LEVEL_DEBUG: return "DEBUG";
    case LOGGER_LEVEL_ERROR: return "ERROR";
    case LOGGER_LEVEL_INFO: return "INFO";
    case LOGGER_LEVEL_WARNING: return "WARNING";
    default: return "UNKOWN";
    }
}

enum LOGGER_LEVEL logger_str_to_level(const char *level) {
    if      (strcmp(level, "DEBUG"  )==0) return LOGGER_LEVEL_DEBUG;
    else if (strcmp(level, "ERROR"  )==0) return LOGGER_LEVEL_ERROR;
    else if (strcmp(level, "INFO"   )==0) return LOGGER_LEVEL_INFO;
    else if (strcmp(level, "WARNING")==0) return LOGGER_LEVEL_WARNING;
    return LOGGER_LEVEL_NONE;
}

void logger_log(logger_t *log, enum LOGGER_LEVEL level, const char *message, ...) {
    if (level > log->level) return;

    char *msgbuf;
    size_t msglen = 0;

    // first figure out how long the formatted message will be
    va_list args;
    va_start(args, message);
    msglen = vsnprintf(NULL, 0, message, args) + 1;
    va_end(args);

    // now allocate it and do the actual format
    msgbuf = egoverlay_calloc(msglen, sizeof(char));
    
    va_start(args, message);
    vsnprintf(msgbuf, msglen, message, args);
    va_end(args);

    struct _timeb tb;
    _ftime_s(&tb);
    struct tm *tmp = localtime(&tb.time);
    char timebuf[24];
    strftime(timebuf, 24, "%Y-%m-%d %T", tmp);

    const char *level_str = logger_level_to_str(level);

    char *outbuf;
    size_t outlen = 0;
    outlen = snprintf(NULL, 0, "%s.%03d | % -20s | % -7s | %s",
                      timebuf, tb.millitm, log->name, level_str, msgbuf) + 1;
    outbuf = egoverlay_calloc(outlen, sizeof(char));
    snprintf(outbuf, outlen, "%s.%03d | % -20s | % -7s | %s",
             timebuf, tb.millitm, log->name, level_str, msgbuf);

    egoverlay_free(msgbuf);

    struct sink_list_node *s = log->sinks;
    while (s) {
        log_sink_write(s->sink, level, outbuf);
        s = s->next;
    }

    egoverlay_free(outbuf);
}

void logger_add_sink(logger_t *log, log_sink_t *sink) {
    struct sink_list_node *node = egoverlay_calloc(1, sizeof(struct sink_list_node));
    node->next = NULL;
    node->sink = sink;

    if (log->sinks==NULL) {
        log->sinks = node;
        return;
    }

    struct sink_list_node *s = log->sinks;
    while (s->next) s = s->next;

    s->next = node;
}

