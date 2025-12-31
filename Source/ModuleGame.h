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
    Box* aiCar2 = nullptr;      // IA
    Box* aiCar3 = nullptr;      // IA
    Box* aiCar4 = nullptr;      // IA
    Box* aiCar5 = nullptr;      // IA
    Box* aiCar6 = nullptr;      // IA
    Box* aiCar7 = nullptr;      // IA
    Box* aiCar8 = nullptr;      // IA
    Box* aiCar9 = nullptr;      // IA
    Box* aiCar10 = nullptr;      // IA
    Box* aiCar11 = nullptr;      // IA

    // ---------- CHECKPOINTS ----------
    // Checkpoints (sensores)
    std::vector<PhysBody*> checkpoints;

    // Progreso player / IA
    int nextCheckpoint = 0; 
    int lapCount = 0;

    int aiNextCheckpoint = 0;
    int aiLapCount = 0;

    // ---------- ASSETS ----------
    Texture2D carTexture;
    Texture2D mapaMontmelo;

    uint32 bonus_fx;

    // ---------- GASOLINE SYSTEM ----------
    float gasoline =1000000.0f;
    const int max_gasoline = 1000000;
    const float gasoline_drain_rate = 16.5f;  // units per second (increased for visibility)

    // ---------- DEBUG / OTROS ----------
    PhysBody* sensor = nullptr;
    bool sensed = false;

    vec2<int> ray;
    bool ray_on = false;


    struct CPDebugRect { int x, y, w, h; };
    std::vector<CPDebugRect> checkpointDebug;

    // debugg colissionessssss bb
    struct DebugPoint
    {
        int x = 0;
        int y = 0;
        float angle = 0.0f;
    };

    std::vector<DebugPoint> savedPoints;


    std::set<std::set<PhysicEntity*>> collidingEntities;
};
