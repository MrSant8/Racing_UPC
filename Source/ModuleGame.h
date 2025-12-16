#pragma once

#include "Globals.h"
#include "Module.h"

#include "p2Point.h"

#include "raylib.h"
#include <vector>
#include <set>

class PhysBody;
class PhysicEntity;


class ModuleGame : public Module
{
public:
	ModuleGame(Application* app, bool start_enabled = true);
	~ModuleGame();

	bool Start();
	update_status Update();
	bool CleanUp();
	void OnCollision(PhysBody* bodyA, PhysBody* bodyB);

public:

	std::vector<PhysicEntity*> entities;
	
	PhysBody* sensor;
	bool sensed;

	Texture2D circle;
	Texture2D box;
	Texture2D mapaMontmelo;
	Texture2D rick;
	Texture2D carTexture;
	PhysicEntity* car;
	Texture2D frontCarTexture;
	Texture2D frontCarTextureLeft;
	Texture2D frontCarTextureRight;
	PhysBody* checkP1 = nullptr;
	PhysBody* checkP2 = nullptr;
	PhysBody* checkP3 = nullptr;

	int nextCheckpoint = 1;
	int lapCount = 0;

	uint32 bonus_fx;

	vec2<int> ray;
	bool ray_on;

	std::set<std::set<PhysicEntity*>> collidingEntities;
};
