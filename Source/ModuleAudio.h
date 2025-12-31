#pragma once

#include "Module.h"

#define MAX_SOUNDS	16
#define DEFAULT_MUSIC_FADE_TIME 2.0f

class ModuleAudio : public Module
{
public:

	ModuleAudio(Application* app, bool start_enabled = true);
	~ModuleAudio();

	bool Init();
	bool CleanUp();

	// Play a music file
	bool PlayMusic(const char* path, float fade_time = DEFAULT_MUSIC_FADE_TIME);

	// Stop current music
	bool StopMusic();

	// Play motor sound (loop while W/S is held)
	bool PlayMotor(const char* path);
	bool StopMotor();

	// Load a sound in memory
	unsigned int LoadFx(const char* path);

	// Play a previously loaded sound
	bool PlayFx(unsigned int fx, int repeat = 0);

	// Update music stream each frame
	update_status Update() override;

private:

	Music music;
	Music motorMusic;
	bool motor_playing = false;
	Sound fx[MAX_SOUNDS];
	unsigned int fx_count;
};
