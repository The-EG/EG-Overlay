#pragma once
#include "logger.h"

logger_t *stderr_logger_new(const char *name, enum LOGGER_LEVEL level);