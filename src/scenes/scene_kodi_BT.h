#pragma once

#include <string>
#include <stdint.h>

extern uint16_t SCENE_KODI_BT;
// FORCE sends the start sequence again even if scene is already active
extern uint16_t SCENE_KODI_BT_FORCE;

extern std::string scene_name_kodi_BT;
void register_scene_kodi_BT(void);
