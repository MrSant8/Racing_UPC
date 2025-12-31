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

    bool Start() override;
    update_status Update() override;
    bool CleanUp() override;
    void OnCollision(PhysBody* bodyA, PhysBody* bodyB) override;

public:
    // ---------- ENTIDADES ----------
    std::vector<PhysicEntity*> entities;

    // ---------- PLAYER ----------
    Box* car = nullptr;

    // ---------- IA (N coches) ----------
    std::vector<Box*> aiCars;     // punteros a las IA
    std::vector<int> aiNextCP;    // next checkpoint por IA
    std::vector<int> aiLaps;      // laps por IA

    // ---------- CHECKPOINTS ----------
    std::vector<PhysBody*> checkpoints;

    // Progreso player
    int nextCheckpoint = 0;
    int lapCount = 0;

    // ---------- ASSETS ----------
    Texture2D carTexture{};
    Texture2D mapaMontmelo{};
    uint32 bonus_fx = 0;

    // ---------- GASOLINE ----------
    float gasoline = 1000000.0f;
    const int max_gasoline = 1000000;
    const float gasoline_drain_rate = 16.5f;

    // ---------- DEBUG ----------
    struct DebugPoint
    {
        int x = 0;
        int y = 0;
        float angle = 0.0f;
    };
    std::vector<DebugPoint> savedPoints;

    PhysBody* sensor = nullptr;
    bool sensed = false;

    vec2<int> ray;
    bool ray_on = false;

    std::set<std::set<PhysicEntity*>> collidingEntities;
};
