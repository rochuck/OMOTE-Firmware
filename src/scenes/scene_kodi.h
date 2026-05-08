#pragma once

#include <string>
#include <stdint.h>

extern uint16_t SCENE_KODI;
// FORCE sends the start sequence again even if scene is already active
extern uint16_t SCENE_KODI_FORCE;

extern std::string scene_name_kodi;
void register_scene_kodi_commands(void);
void register_scene_kodi(void);
