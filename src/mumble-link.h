#pragma once
#include <stdint.h>

typedef enum mumble_link_profession_t {
    MUMBLE_LINK_PROFESSION_ERROR = -1,
    MUMBLE_LINK_PROFESSION_GUARDIAN = 1,
    MUMBLE_LINK_PROFESSION_WARRIOR,
    MUMBLE_LINK_PROFESSION_ENGINEER,
    MUMBLE_LINK_PROFESSION_RANGER,
    MUMBLE_LINK_PROFESSION_THIEF,
    MUMBLE_LINK_PROFESSION_ELEMENTALIST,
    MUMBLE_LINK_PROFESSION_MESMER,
    MUMBLE_LINK_PROFESSION_NECROMANCER,
    MUMBLE_LINK_REVENANT
} mumble_link_profession_t;

#define MUMBLE_LINK_UI_STATE_MAP_OPEN           0x01
#define MUMBLE_LINK_UI_STATE_COMPASS_TOP_RIGHT (0x01 << 1)
#define MUMBLE_LINK_UI_STATE_COMPASS_ROTATE    (0x01 << 2)
#define MUMBLE_LINK_UI_STATE_GAME_FOCUS        (0x01 << 3)
#define MUMBLE_LINK_UI_STATE_COMP_MODE         (0x01 << 4)
#define MUMBLE_LINK_UI_STATE_TEXTBOX_FOCUS     (0x01 << 5)
#define MUMBLE_LINK_UI_STATE_IN_COMBAT         (0x01 << 6)

typedef enum mumble_link_ui_size_t {
    MUMBLE_LINK_UI_SIZE_SMALL = 0,
    MUMBLE_LINK_UI_SIZE_NORMAL,
    MUMBLE_LINK_UI_SIZE_LARGE,
    MUMBLE_LINK_UI_SIZE_LARGER
} mumble_link_ui_size_t;

void mumble_link_init();

uint32_t mumble_link_tick();
uint32_t mumble_link_version();

size_t mumble_link_name(char *name, size_t max_size);
size_t mumble_link_identity(char *identity, size_t max_size);

void mumble_link_avatar_position(float *x, float *y, float *z);
void mumble_link_camera_position(float *x, float *y, float *z);
void mumble_link_camera_front(float *x, float *y, float *z);

float mumble_link_fov();

size_t mumble_link_character_name(char *name, size_t max_size);
mumble_link_profession_t mumble_link_character_profression();

void mumble_link_map_center(float *x, float *y);
void mumble_link_map_size(uint16_t *width, uint16_t *height);
float mumble_link_map_scale();
float mumble_link_map_rotation();

uint32_t mumble_link_ui_state();

mumble_link_ui_size_t mumble_link_ui_size();

void mumble_link_cleanup();
