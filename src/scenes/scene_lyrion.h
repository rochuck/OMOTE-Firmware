#pragma once

#include <stdint.h>
#include <string>

extern uint16_t SCENE_LYRION;
// FORCE sends the start sequence again even if scene is already active
extern uint16_t SCENE_LYRION_FORCE;

extern std::string scene_name_lyrion;
void register_scene_lyrion(void);
