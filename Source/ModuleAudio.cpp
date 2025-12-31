#include "Globals.h"
#include "Application.h"
#include "ModuleAudio.h"

#include "raylib.h"

// Default volumes
static const float MUSIC_DEFAULT_VOLUME =0.1f;
static const float MOTOR_DEFAULT_VOLUME =0.6f;
static const float FX_DEFAULT_VOLUME =0.8f;

ModuleAudio::ModuleAudio(Application* app, bool start_enabled) : Module(app, start_enabled)
{
	fx_count =0;
	music = Music{0};
	// motorMusic declared in header; initialize here
	motorMusic = Music{0};
	motor_playing = false;
}

// Destructor
ModuleAudio::~ModuleAudio()
{
}

// Called before render is available
bool ModuleAudio::Init()
{
	LOG("Loading Audio Mixer");
	bool ret = true;

	LOG("Loading raylib audio system");

	InitAudioDevice();

	return ret;
}

// Called before quitting
bool ModuleAudio::CleanUp()
{
	LOG("Freeing sound FX, closing Mixer and Audio subsystem");

	// Unload sounds
	for (unsigned int i =0; i < fx_count; i++)
	{
		UnloadSound(fx[i]);
	}

	// Unload music
	if (IsMusicReady(music))
	{
		StopMusicStream(music);
		UnloadMusicStream(music);
		music = Music{0};
	}

	// Unload motor music if any
	if (IsMusicReady(motorMusic))
	{
		StopMusicStream(motorMusic);
		UnloadMusicStream(motorMusic);
		motorMusic = Music{0};
		motor_playing = false;
	}

	CloseAudioDevice();

	return true;
}

// Play a music file
bool ModuleAudio::PlayMusic(const char* path, float fade_time)
{
	if (IsEnabled() == false)
		return false;

	// Stop any currently playing music and unload it
	if (IsMusicReady(music))
	{
		StopMusicStream(music);
		UnloadMusicStream(music);
		music = Music{0};
	}

	music = LoadMusicStream(path);
	if (!IsMusicReady(music))
	{
		LOG("Cannot load music: %s", path);
		return false;
	}

	// ensure music will loop until explicitly stopped (many raylib versions expose this field)
	// Set it if available
	#ifdef MUSIC_LOOPING
	music.looping = true;
	#else
	// try to set looping member anyway; if not present compilers that support it will accept it
	music.looping = true;
	#endif

	PlayMusicStream(music);
	// Kick the stream once so playback starts immediately
	UpdateMusicStream(music);

	// Set default music volume (lowered)
	SetMusicVolume(music, MUSIC_DEFAULT_VOLUME);

	LOG("Successfully started music %s", path);

	return true;
}

// Stop current music and unload
bool ModuleAudio::StopMusic()
{
	if (!IsMusicReady(music)) return false;

	StopMusicStream(music);
	UnloadMusicStream(music);
	music = Music{0};
	return true;
}

// Play motor sound (loop while W/S is held)
bool ModuleAudio::PlayMotor(const char* path)
{
	if (IsEnabled() == false) return false;
	if (motor_playing) return true;

	// Stop/unload existing motor music if any
	if (IsMusicReady(motorMusic))
	{
		StopMusicStream(motorMusic);
		UnloadMusicStream(motorMusic);
		motorMusic = Music{0};
	}

	motorMusic = LoadMusicStream(path);
	if (!IsMusicReady(motorMusic))
	{
		LOG("Cannot load motor music: %s", path);
		return false;
	}

	// Request looping
	motorMusic.looping = true;
	PlayMusicStream(motorMusic);
	UpdateMusicStream(motorMusic);
	// Slightly louder than background but not full
	SetMusicVolume(motorMusic,0.6f);
	motor_playing = true;
	return true;
}

// Stop motor music
bool ModuleAudio::StopMotor()
{
	if (!motor_playing) return false;
	if (IsMusicReady(motorMusic))
	{
		StopMusicStream(motorMusic);
		UnloadMusicStream(motorMusic);
		motorMusic = Music{0};
	}
	motor_playing = false;
	return true;
}

// Update music stream each frame
update_status ModuleAudio::Update()
{
	// Keep streaming music if loaded
	if (IsMusicReady(music))
	{
		UpdateMusicStream(music);
	}
	// Update motor music stream as well
	if (IsMusicReady(motorMusic))
	{
		UpdateMusicStream(motorMusic);
	}
	return UPDATE_CONTINUE;
}

// Load WAV
unsigned int ModuleAudio::LoadFx(const char* path)
{
	if (IsEnabled() == false)
		return 0;

	unsigned int ret =0;

	Sound sound = LoadSound(path);

	if (sound.stream.buffer == NULL)
	{
		LOG("Cannot load sound: %s", path);
	}
	else
	{
		fx[fx_count] = sound;
		ret = fx_count++;
		// set default fx volume
		SetSoundVolume(fx[ret], FX_DEFAULT_VOLUME);
	}

	return ret;
}

// Play WAV
bool ModuleAudio::PlayFx(unsigned int id, int repeat)
{
	if (IsEnabled() == false)
	{
		return false;
	}

	bool ret = false;

	if (id < fx_count) PlaySound(fx[id]);

	return ret;
}