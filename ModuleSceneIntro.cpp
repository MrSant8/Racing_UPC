#include "Globals.h"
#include "Application.h"
#include "ModuleSceneIntro.h"

ModuleSceneIntro::ModuleSceneIntro(Application* app, bool start_enabled) : Module(app, start_enabled)
{
}

ModuleSceneIntro::~ModuleSceneIntro()
{
}

bool ModuleSceneIntro::Start()
{
	LOG("Loading Intro Scene");

	// Carga la imagen desde la carpeta Assets
	// Asegúrate de que el nombre coincida EXACTAMENTE
	background = LoadTexture("Assets/mapa_montmelo.png");

	return true;
}

update_status ModuleSceneIntro::Update()
{
	// Pinta la textura en la posición 0,0 de la pantalla
	// WHITE significa que no le aplicamos tinte de color
	DrawTexture(background, 0, 0, WHITE);

	return UPDATE_CONTINUE;
}

bool ModuleSceneIntro::CleanUp()
{
	LOG("Unloading Intro Scene");
	UnloadTexture(background); // ¡Importante limpiar memoria!
	return true;
}