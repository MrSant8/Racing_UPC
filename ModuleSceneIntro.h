#pragma once
#include "Module.h"
#include "p2Point.h"
#include "Globals.h"
#include "raylib.h" 

class ModuleSceneIntro : public Module
{
public:
	ModuleSceneIntro(Application* app, bool start_enabled = true);
	~ModuleSceneIntro();

	bool Start();
	update_status Update();
	bool CleanUp();

public:
	Texture2D background; // Aquí guardaremos la imagen
};