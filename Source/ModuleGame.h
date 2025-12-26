#pragma once

#include "Globals.h"
#include "Module.h"

#include "p2Point.h"
#include "raylib.h"

#include <vector>
#include <set>

class PhysBody;
class PhysicEntity;
class Box;  

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
    // ---------- ENTIDADES ----------
    std::vector<PhysicEntity*> entities;

    // ---------- COCHES ----------
    Box* car = nullptr;        // jugador
    Box* aiCar = nullptr;      // IA

    // ---------- CHECKPOINTS ----------
    PhysBody* checkP1 = nullptr;
    PhysBody* checkP2 = nullptr;
    PhysBody* checkP3 = nullptr;

    int nextCheckpoint = 1;
    int lapCount = 0;

    int aiNextCheckpoint = 1;
    int aiLapCount = 0;

    // ---------- ASSETS ----------
    Texture2D carTexture;
    Texture2D mapaMontmelo;

    uint32 bonus_fx;

    // ---------- DEBUG / OTROS ----------
    PhysBody* sensor = nullptr;
    bool sensed = false;

    vec2<int> ray;
    bool ray_on = false;

    std::set<std::set<PhysicEntity*>> collidingEntities;
};
