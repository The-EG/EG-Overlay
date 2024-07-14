#pragma once
#include <lua.h>
#include <jansson.h>

typedef struct settings_t settings_t;

settings_t *settings_new(const char *name);

void settings_ref(settings_t *settings);
void settings_unref(settings_t *settings);

void settings_lua_init();

int settings_has_value(settings_t *settings, const char *key);

int settings_get        (settings_t *settings, const char *key, json_t **value);
int settings_get_int    (settings_t *settings, const char *key, int *value);
int settings_get_string (settings_t *settings, const char *key, const char **value);
int settings_get_double (settings_t *settings, const char *key, double *value);
int settings_get_boolean(settings_t *settings, const char *key, int *value);

void settings_set        (settings_t *settings, const char *key, json_t *value);
void settings_set_int    (settings_t *settings, const char *key, int value);
void settings_set_string (settings_t *settings, const char *key, const char *value);
void settings_set_double (settings_t *settings, const char *key, double value);
void settings_set_boolean(settings_t *settings, const char *key, int value);

void settings_set_default        (settings_t *settings, const char *key, json_t *value);
void settings_set_default_int    (settings_t *settings, const char *key, int value);
void settings_set_default_string (settings_t *settings, const char *key, const char *value);
void settings_set_default_double (settings_t *settings, const char *key, double value);
void settings_set_default_boolean(settings_t *settings, const char *key, int value);

void settings_save(settings_t *settings);

void lua_pushsettings(lua_State *L, settings_t *settings);

settings_t *lua_checksettings(lua_State *L, int ind);
