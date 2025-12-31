#include "Globals.h"
#include "Application.h"
#include "ModuleGame.h"
#include "ModuleAudio.h"
#include "ModulePhysics.h"
#include "ModuleRender.h"
#include <vector>
#include <fstream>
#include <cstdio>   // snprintf
#include "raylib.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <ctime>

static Texture2D gFrontCarTexture;
static Texture2D gFrontCarTextureLeft;
static Texture2D gFrontCarTextureRight;

// =====================================================
// TIMERS (NO TOCAN EL .H) - Jugador + IAs
// =====================================================
static const int kMaxLaps = 3;
static bool sRaceFinished = false;

static float sPlayerLapStart = 0.0f;
static float sPlayerLapCurrent = 0.0f;
static float sPlayerLapLast = 0.0f;
static float sPlayerLapBest = 999999.0f;

static std::vector<float> sAiLapStart;
static std::vector<float> sAiLapCurrent;
static std::vector<float> sAiLapLast;
static std::vector<float> sAiLapBest;

// ===== END GAME STATE =====
static bool sAiWon = false;
static float sEndTime = 0.0f;      
static const float kEndDelay = 3.0f; 

static void FormatTime(float t, char* out, int outSize)
{
    if (t < 0.0f) t = 0.0f;
    int minutes = (int)(t / 60.0f);
    float seconds = t - minutes * 60.0f;
    std::snprintf(out, outSize, "%02d:%05.2f", minutes, seconds);
}

static void DrawRaceTimesHUD(int lapCount,
    const std::vector<int>& aiLaps,
    int numAI)
{
    char cur[32], best[32], last[32];
    FormatTime(sPlayerLapCurrent, cur, 32);
    FormatTime((sPlayerLapBest >= 999998.0f) ? 0.0f : sPlayerLapBest, best, 32);
    FormatTime(sPlayerLapLast, last, 32);

    int x = 30;
    int y = 70;

    DrawText(TextFormat("Vuelta: %d/%d", lapCount, kMaxLaps), x, y, 20, WHITE); y += 22;
    DrawText(TextFormat("Lap:  %s", cur), x, y, 20, WHITE); y += 22;
    DrawText(TextFormat("Best: %s", best), x, y, 20, WHITE); y += 22;
    DrawText(TextFormat("Last: %s", last), x, y, 20, WHITE); y += 28;

    DrawText("IAS (lap | best)", x, y, 18, WHITE); y += 22;

    for (int i = 0; i < numAI; ++i)
    {
        char aiBestStr[32];
        float b = (i < (int)sAiLapBest.size() && sAiLapBest[i] < 999998.0f) ? sAiLapBest[i] : 0.0f;
        FormatTime(b, aiBestStr, 32);

        int laps = (i < (int)aiLaps.size()) ? aiLaps[i] : 0;
        DrawText(TextFormat("AI%02d: %d | %s", i + 1, laps, aiBestStr), x, y, 18, WHITE);
        y += 20;

        if (y > SCREEN_HEIGHT - 40) break;
    }
}

// =====================================================================
// CLASE BASE: cualquier cosa con física que se pueda dibujar
// =====================================================================
class PhysicEntity
{
protected:
    PhysicEntity(PhysBody* _body, Module* _listener)
        : body(_body)
        , listener(_listener)
    {
        body->listener = listener;
    }

public:
    virtual ~PhysicEntity() = default;
    virtual void Update() = 0;

public:
    PhysBody* body = nullptr;
    Module* listener = nullptr;
};

// =====================================================================
// COCHE F1 (jugador o IA)
// =====================================================================
class Box : public PhysicEntity
{
public:
    Box(ModulePhysics* physics, int _x, int _y, Module* _listener,
        Texture2D bodyTex, Texture2D frontTex,
        bool ai = false, int _aiId = -1)
        : PhysicEntity(physics->CreateRectangle(_x, _y, 90, 40), _listener)
        , bodyTexture(bodyTex)
        , frontTexture(frontTex)
        , isAI(ai)
        , aiId(_aiId)
    {
    }

    void Update() override
    {
        if (isAI) UpdateAI();
        else HandleInput();

        UpdateMovement();
        DrawCar();
    }

    bool IsAI() const { return isAI; }

private:
    int aiId = -1;

    // ---------------------- INPUT WASD (JUGADOR) ----------------------
    void HandleInput()
    {
        forwardInput = 0.0f;
        steeringInput = 0.0f;

        // Space hard brake
        if (IsKeyDown(KEY_SPACE))
            hardBrake = true;
        else
            hardBrake = false;

        if (IsKeyDown(KEY_W)) forwardInput = 1.1f;
        if (IsKeyDown(KEY_S)) forwardInput = -1.1f;
        if (IsKeyDown(KEY_A)) steeringInput = -1.0f;
        if (IsKeyDown(KEY_D)) steeringInput = 1.0f;

        // If hard braking, cancel forward input so we don't accelerate
        if (hardBrake)
            forwardInput = 0.0f;
    }

    // ---------------------- IA SIMPLE: seguir checkP1->checkP2->... ----------------------
    void UpdateAI()
    {
        steeringInput = 0.0f;

        // Small random chance (approx 1%) the AI will slow down / reverse
        int r = std::rand() % 100 + 1; // 1..100
        if (r == 1)
            forwardInput = -10.0f;
        else
            forwardInput = 1.0f;

        int x, y;
        body->GetPhysicPosition(x, y);

        ModuleGame* game = (ModuleGame*)listener;

        PhysBody* target = nullptr;
        if (!game->checkpoints.empty())
        {
            int idx = 0;
            if (aiId >= 0 && aiId < (int)game->aiNextCP.size())
                idx = game->aiNextCP[aiId];
            if (idx < 0) idx = 0;
            if (idx >= (int)game->checkpoints.size()) idx = 0;
            target = game->checkpoints[idx];
        }
        if (target == nullptr) return;

        int tx, ty;
        target->GetPhysicPosition(tx, ty);

        float dx = (float)(tx - x);
        float dy = (float)(ty - y);

        float targetAngle = atan2f(dy, dx);
        float currentAngle = body->GetRotation();

        float diff = targetAngle - currentAngle;

        // Normalizar a (-PI, PI)
        while (diff > PI)  diff -= 2.0f * PI;
        while (diff < -PI) diff += 2.0f * PI;

        if (diff > 0.08f) steeringInput = 0.6f;
        else if (diff < -0.08f) steeringInput = -0.6f;
        else steeringInput = 0.0f;
    }

    // ---------------- VELOCIDAD / FRENADO / GIRO ----------------
    void UpdateMovement()
    {
        float angle = body->GetRotation();
        b2Vec2 posMeters = body->body->GetPosition();

        // Get game module to check gasoline
        ModuleGame* game = (ModuleGame*)listener;
        float dt = GetFrameTime();

        // ---- REGENERATE IF INSIDE BROWN RECTANGLE (world coords) ----
        if (!isAI)
        {
            int car_px, car_py;
            body->GetPhysicPosition(car_px, car_py);

            const int brown1_x = 13360;
            const int brown1_y = 5100;
            const int brown2_x = 5800;
            const int brown2_y = 4230;
            const int brown_w = 300;
            const int brown_h = 150;

            bool insideBrown = false;
            if (car_px >= brown1_x && car_px <= brown1_x + brown_w && car_py >= brown1_y && car_py <= brown1_y + brown_h)
                insideBrown = true;
            else if (car_px >= brown2_x && car_px <= brown2_x + brown_w && car_py >= brown2_y && car_py <= brown2_y + brown_h)
                insideBrown = true;

            const float stop_threshold = 0.05f;
            if (insideBrown && fabsf(speedCar) < stop_threshold && forwardInput == 0.0f)
            {
                // OJO: esto depende de que gasolina exista en ModuleGame (como en tu código original)
                game->gasoline += 20.0f * dt;
                if (game->gasoline > (float)game->max_gasoline) game->gasoline = (float)game->max_gasoline;
            }
        }
        // ---- ACELERAR / FRENAR ----
        bool canAccelerate = true;
        if (!isAI) canAccelerate = (game->gasoline > 0.0f);

        bool inputMoving = (forwardInput > 0.0f || forwardInput < 0.0f);

        if (isAI)
        {
            if (forwardInput > 0.0f)
            {
                speedCar += acceleration;
                if (speedCar > maxSpeed) speedCar = maxSpeed;
            }
            else if (forwardInput < 0.0f)
            {
                speedCar -= acceleration;
                if (speedCar < -maxSpeed) speedCar = -maxSpeed;
            }
        }
        else
        {
            // Hard brake when player presses SPACE
            const float hardBrakePower = 0.6f; // strong deceleration per frame
            if (hardBrake)
            {
                if (speedCar > 0.0f)
                {
                    speedCar -= hardBrakePower;
                    if (speedCar < 0.0f) speedCar = 0.0f;
                }
                else if (speedCar < 0.0f)
                {
                    speedCar += hardBrakePower;
                    if (speedCar > 0.0f) speedCar = 0.0f;
                }
            }
            else if (inputMoving && canAccelerate)
            {
                if (forwardInput > 0.0f)
                {
                    speedCar += acceleration;
                    if (speedCar > maxSpeed) speedCar = maxSpeed;
                }
                else if (forwardInput < 0.0f)
                {
                    speedCar -= acceleration;
                    if (speedCar < -maxSpeed) speedCar = -maxSpeed;
                }

                // Drain gasoline while player is pressing throttle
                game->gasoline -= game->gasoline_drain_rate * dt;
                if (game->gasoline < 0.0f) game->gasoline = 0.0f;
            }
            else
            {
                if (speedCar > 0.0f)
                {
                    speedCar -= braking;
                    if (speedCar < 0.0f) speedCar = 0.0f;
                }
                else if (speedCar < 0.0f)
                {
                    speedCar += braking;
                    if (speedCar > 0.0f) speedCar = 0.0f;
                }
            }
        }

        // ---- GIRO DEL COCHE ----
        float speedFactor = fabsf(speedCar);
        float baseTurnSpeedRad = baseTurnSpeedDeg * DEGTORAD;

        angle += steeringInput * baseTurnSpeedRad * speedFactor;

        if (steeringInput != 0.0f)
        {
            steeringVisual += steeringInput * steerVisualSpeed;
            if (steeringVisual > maxSteerVisualDeg) steeringVisual = maxSteerVisualDeg;
            if (steeringVisual < -maxSteerVisualDeg) steeringVisual = -maxSteerVisualDeg;
        }
        else
        {
            steeringVisual *= 0.85f;
        }

        // ---- APLICAR A BOX2D ----
        body->body->SetTransform(posMeters, angle);

        b2Vec2 vel;
        vel.x = cosf(angle) * speedCar * moveFactor;
        vel.y = sinf(angle) * speedCar * moveFactor;
        body->body->SetLinearVelocity(vel);
    }

    // ---------------------- DIBUJAR COCHE ----------------------
    void DrawCar()
    {
        int x, y;
        body->GetPhysicPosition(x, y);
        float angle = body->GetRotation();
        float angleDeg = angle * RAD2DEG;

        // --- CÁMARA (offset pantalla) ---
        ModuleGame* game = (ModuleGame*)listener;
        float camX = game->App->renderer->camera.x;
        float camY = game->App->renderer->camera.y;

        float scale = 0.05f;

        // ===== CARROCERÍA =====
        Rectangle srcBody = { 0, 0, (float)bodyTexture.width, (float)bodyTexture.height };
        Rectangle dstBody = {
            (float)x + camX, (float)y + camY,
            bodyTexture.width * scale,
            bodyTexture.height * scale
        };
        Vector2 originBody = { dstBody.width / 2.0f, dstBody.height / 2.0f };

        DrawTexturePro(bodyTexture, srcBody, dstBody, originBody, angleDeg, WHITE);

        // ===== MORRO =====
        Texture2D* frontTex = &gFrontCarTexture;
        if (steeringInput < -0.1f) frontTex = &gFrontCarTextureLeft;
        else if (steeringInput > 0.1f) frontTex = &gFrontCarTextureRight;

        float bodyW = dstBody.width;
        float forwardOffset = bodyW * 0.00f;

        float cosA = cosf(angle);
        float sinA = sinf(angle);

        Vector2 frontPos;
        frontPos.x = (float)x + camX + cosA * forwardOffset;
        frontPos.y = (float)y + camY + sinA * forwardOffset;

        Rectangle srcFront = { 0, 0, (float)frontTex->width, (float)frontTex->height };
        Rectangle dstFront = {
            frontPos.x, frontPos.y,
            frontTex->width * scale,
            frontTex->height * scale
        };
        Vector2 originFront = { dstFront.width / 2.0f, dstFront.height / 2.0f };

        DrawTexturePro(*frontTex, srcFront, dstFront, originFront, angleDeg, WHITE);
    }

private:
    Texture2D bodyTexture;
    Texture2D frontTexture;

    bool isAI = false;

    float forwardInput = 0.0f;
    float steeringInput = 0.0f;

    float speedCar = 0.0f;
    float steeringVisual = 0.0f;

    const float acceleration = 0.115f;
    const float braking = 0.020f;
    const float maxSpeed = 12.0f;

    const float moveFactor = 2.0f;
    const float baseTurnSpeedDeg = 0.35f;

    const float maxSteerVisualDeg = 12.0f;
    const float steerVisualSpeed = 1.5f;
    bool hardBrake = false;

};

// =====================================================================
// MODULE GAME
// =====================================================================

ModuleGame::ModuleGame(Application* app, bool start_enabled) : Module(app, start_enabled)
{
    ray_on = false;
    sensed = false;
    lapCount = 0;
    nextCheckpoint = 1;
}

ModuleGame::~ModuleGame()
{
}

bool ModuleGame::Start()
{
    LOG("Loading Game assets");
    bool ret = true;

    App->renderer->camera.x = App->renderer->camera.y = 0;

    // mapa
    mapaMontmelo = LoadTexture("Assets/mapa_montmelo.png");

    // Texturas del coche
    carTexture = LoadTexture("Assets/f1_body_car.png");
    gFrontCarTexture = LoadTexture("Assets/f1_front_car.png");
    gFrontCarTextureLeft = LoadTexture("Assets/f1_front_car_Left.png");
    gFrontCarTextureRight = LoadTexture("Assets/f1_front_car_Right.png");

    bonus_fx = App->audio->LoadFx("Assets/bonus.wav");

    entities.clear();

    // seed randomness for AI behavior
    std::srand((unsigned)std::time(nullptr));

    // Coche jugador
    car = new Box(App->physics,
        10779,
        5460,
        this,
        carTexture,
        gFrontCarTexture,
        false);

    entities.emplace_back(car);

    b2Vec2 pos = car->body->body->GetPosition();
    car->body->body->SetTransform(pos, PI);

    car->body->body->SetFixedRotation(true);

    // --- limpiar IA ---
    aiCars.clear();
    aiNextCP.clear();
    aiLaps.clear();

    const int NUM_AI = 10;

    for (int i = 0; i < NUM_AI; ++i)
    {
        int spawnX = 10779 + (i + 1) * 80;
        int spawnY = 5460 + (i % 2) * 60;

        Box* ai = new Box(App->physics,
            spawnX, spawnY,
            this,
            carTexture,
            gFrontCarTexture,
            true,
            i
        );

        entities.emplace_back(ai);
        aiCars.push_back(ai);

        aiNextCP.push_back(0);
        aiLaps.push_back(0);

        b2Vec2 posAI = ai->body->body->GetPosition();
        ai->body->body->SetTransform(posAI, PI);

        ai->body->body->SetFixedRotation(true);
        ai->body->body->SetLinearDamping(3.0f);
        ai->body->body->SetAngularDamping(5.0f);
    }

    car->body->body->SetFixedRotation(true);

    // ================= CHECKPOINTS =================
    struct CheckpointData { int x, y, w, h; };

    std::vector<CheckpointData> cpData =
    {
        {10779, 5460, 300, 120}, // 00
        {9733, 5384, 300, 120},  // 01
        {8119, 5381, 300, 120},  // 02
        {6889, 5378, 300, 120},  // 03
        {5792, 5404, 300, 120},  // 04
        {4111, 5370, 300, 120},  // 05
        {3463, 5348, 300, 120},  // 06
        {3244, 5293, 300, 120},  // 07
        {3117, 5174, 120, 300},  // 08
        {3041, 5057, 120, 300},  // 09
        {2998, 4859, 120, 300},  // 10
        {2997, 4559, 120, 300},  // 11
        {2953, 4400, 120, 300},  // 12
        {2858, 4244, 120, 300},  // 13
        {2668, 4082, 300, 120},  // 14
        {2407, 3937, 300, 120},  // 15
        {2110, 3829, 300, 120},  // 16
        {1922, 3761, 300, 120},  // 17
        {1658, 3618, 300, 120},  // 18
        {1454, 3476, 300, 120},  // 19
        {1302, 3323, 120, 300},  // 20
        {1208, 3166, 120, 300},  // 21
        {1131, 2947, 120, 300},  // 22
        {1116, 2714, 120, 300},  // 23
        {1155, 2367, 120, 300},  // 24
        {1251, 2103, 120, 300},  // 25
        {1449, 1836, 120, 300},  // 26
        {1752, 1529, 300, 120},  // 27
        {2111, 1288, 300, 120},  // 28
        {2405, 1174, 300, 120},  // 29
        {2679, 1102, 300, 120},  // 30
        {2944, 1079, 300, 120},  // 31
        {3327, 1072, 300, 120},  // 32
        {3910, 1061, 300, 120},  // 33
        {4375, 1080, 300, 120},  // 34
        {5275, 1063, 300, 120},  // 35
        {5604, 1096, 300, 120},  // 36
        {5820, 1183, 300, 120},  // 37
        {6013, 1390, 120, 300},  // 38
        {6087, 1499, 120, 300},  // 39
        {6154, 1669, 120, 300},  // 40
        {6175, 1818, 120, 300},  // 41
        {6139, 1997, 120, 300},  // 42
        {6057, 2141, 120, 300},  // 43
        {5953, 2272, 120, 300},  // 44
        {5810, 2410, 300, 120},  // 45
        {5653, 2504, 300, 120},  // 46
        {5437, 2590, 300, 120},  // 47
        {5240, 2617, 300, 120},  // 48
        {4990, 2628, 300, 120},  // 49
        {4524, 2649, 300, 120},  // 50
        {3892, 2657, 300, 120},  // 51
        {3276, 2684, 300, 120},  // 52
        {2993, 2703, 300, 120},  // 53
        {2890, 2756, 300, 120},  // 54
        {2805, 2877, 120, 300},  // 55
        {2797, 3025, 120, 300},  // 56
        {2837, 3238, 120, 300},  // 57
        {2927, 3375, 300, 120},  // 58
        {3102, 3471, 300, 120},  // 59
        {3367, 3643, 300, 120},  // 60
        {3651, 3846, 300, 120},  // 61
        {3822, 3950, 300, 120},  // 62
        {3974, 4053, 300, 120},  // 63
        {4114, 4143, 300, 120},  // 64
        {4337, 4256, 300, 120},  // 65
        {4503, 4334, 300, 120},  // 66
        {4678, 4386, 300, 120},  // 67
        {4854, 4437, 300, 120},  // 68
        {5083, 4474, 300, 120},  // 69
        {5316, 4484, 300, 120},  // 70
        {5550, 4493, 300, 120},  // 71
        {5782, 4490, 300, 120},  // 72
        {5982, 4493, 300, 120},  // 73
        {6198, 4500, 300, 120},  // 74
        {6397, 4490, 300, 120},  // 75
        {6555, 4437, 300, 120},  // 76
        {6664, 4315, 120, 300},  // 77
        {6708, 4189, 120, 300},  // 78
        {6743, 4061, 120, 300},  // 79
        {6760, 3895, 120, 300},  // 80
        {6766, 3645, 120, 300},  // 81
        {6808, 3484, 120, 300},  // 82
        {6897, 3324, 120, 300},  // 83
        {7051, 3087, 120, 300},  // 84
        {7189, 2899, 120, 300},  // 85
        {7298, 2752, 120, 300},  // 86
        {7437, 2564, 120, 300},  // 87
        {7565, 2390, 120, 300},  // 88
        {7694, 2215, 120, 300},  // 89
        {7861, 1967, 120, 300},  // 90
        {8036, 1704, 120, 300},  // 91
        {8186, 1525, 120, 300},  // 92
        {8323, 1404, 300, 120},  // 93
        {8477, 1341, 300, 120},  // 94
        {8658, 1311, 300, 120},  // 95
        {8890, 1328, 300, 120},  // 96
        {9176, 1410, 300, 120},  // 97
        {9375, 1497, 300, 120},  // 98
        {9547, 1599, 300, 120},  // 99
        {9791, 1742, 300, 120},  // 100
        {9949, 1835, 300, 120},  // 101
        {10150, 1953, 300, 120}, // 102
        {10366, 2080, 300, 120}, // 103
        {10653, 2249, 300, 120}, // 104
        {10883, 2384, 300, 120}, // 105
        {11070, 2494, 300, 120}, // 106
        {11314, 2637, 300, 120}, // 107
        {11530, 2764, 300, 120}, // 108
        {11760, 2899, 300, 120}, // 109
        {12018, 3051, 300, 120}, // 110
        {12263, 3195, 300, 120}, // 111
        {12550, 3364, 300, 120}, // 112
        {12809, 3516, 300, 120}, // 113
        {13125, 3701, 300, 120}, // 114
        {13455, 3896, 300, 120}, // 115
        {13700, 4039, 300, 120}, // 116
        {13940, 4148, 300, 120}, // 117
        {14052, 4133, 300, 120}, // 118
        {13969, 3436, 120, 300}, // 119
        {13854, 3172, 120, 300}, // 120
        {13711, 2914, 120, 300}, // 121
        {13591, 2755, 300, 120}, // 122
        {13400, 2655, 300, 120}, // 123
        {13190, 2553, 300, 120}, // 124
        {12998, 2453, 300, 120}, // 125
        {12864, 2309, 120, 300}, // 126
        {12811, 2099, 120, 300}, // 127
        {12833, 1885, 120, 300}, // 128
        {12910, 1648, 120, 300}, // 129
        {13036, 1516, 300, 120}, // 130
        {13251, 1391, 300, 120}, // 131
        {13483, 1372, 300, 120}, // 132
        {13707, 1432, 300, 120}, // 133
        {14003, 1545, 300, 120}, // 134
        {14454, 1718, 300, 120}, // 135
        {14863, 1859, 300, 120}, // 136
        {15190, 1981, 300, 120}, // 137
        {15463, 2136, 120, 300}, // 138
        {15602, 2401, 120, 300}, // 139
        {15648, 2679, 120, 300}, // 140
        {15651, 3063, 120, 300}, // 141
        {15641, 3646, 120, 300}, // 142
        {15630, 4296, 120, 300}, // 143
        {15619, 4595, 120, 300}, // 144
        {15535, 4810, 120, 300}, // 145
        {15419, 4993, 120, 300}, // 146
        {15262, 5165, 300, 120}, // 147
        {15082, 5285, 300, 120}, // 148
        {14879, 5362, 300, 120}, // 149
        {14568, 5405, 300, 120}, // 150
        {13952, 5391, 300, 120}, // 151
        {13636, 5394, 300, 120}, // 152
        {12838, 5382, 300, 120}, // 153
        {12205, 5379, 300, 120}, // 154
    };

    checkpoints.clear();
    for (const auto& cp : cpData)
    {
        PhysBody* sensor = App->physics->CreateRectangleSensor(cp.x, cp.y, cp.w, cp.h);
        sensor->listener = this;
        checkpoints.push_back(sensor);
    }

    // Reset contadores
    lapCount = 0;
    nextCheckpoint = 0;

    // ===== RESET TIMERS (STATIC) =====
    sRaceFinished = false;
    sAiWon = false;
    sEndTime = 0.0f;

    sPlayerLapStart = (float)GetTime();
    sPlayerLapCurrent = 0.0f;
    sPlayerLapLast = 0.0f;
    sPlayerLapBest = 999999.0f;

    sAiLapStart.assign(aiCars.size(), (float)GetTime());
    sAiLapCurrent.assign(aiCars.size(), 0.0f);
    sAiLapLast.assign(aiCars.size(), 0.0f);
    sAiLapBest.assign(aiCars.size(), 999999.0f);

    return ret;
}

bool ModuleGame::CleanUp()
{
    LOG("Unloading Game scene");

    for (PhysicEntity* e : entities)
        delete e;
    entities.clear();

    UnloadTexture(carTexture);
    UnloadTexture(gFrontCarTexture);
    UnloadTexture(gFrontCarTextureLeft);
    UnloadTexture(gFrontCarTextureRight);
    UnloadTexture(mapaMontmelo);

    return true;
}

update_status ModuleGame::Update()
{
    constexpr float MAP_SCALE = 1.0f;

    // Posición del coche
    int carPx = 0, carPy = 0;
    float carAngle = 0.0f;

    if (car != nullptr)
    {
        car->body->GetPhysicPosition(carPx, carPy);
        carAngle = car->body->GetRotation() * RAD2DEG;

    }
    else
    {
        return UPDATE_CONTINUE;
    }

    // ===== CRONOS ACTUALES =====
    sPlayerLapCurrent = (float)GetTime() - sPlayerLapStart;
    for (int i = 0; i < (int)aiCars.size(); ++i)
    {
        if (i < (int)sAiLapStart.size())
            sAiLapCurrent[i] = (float)GetTime() - sAiLapStart[i];
    }

    // ===== GUARDAR COORDENADAS CON TECLA =====
    //if (IsKeyPressed(KEY_P))
    //{
    //    DebugPoint pt;
    //    pt.x = carPx;
    //    pt.y = carPy;
    //    pt.angle = carAngle;

    //    savedPoints.push_back(pt);
    //    LOG("SAVED: X=%d Y=%d A=%.1f (total=%d)", pt.x, pt.y, pt.angle, (int)savedPoints.size());
    //}

    //// O = exportar a archivo cpData.txt
    //if (IsKeyPressed(KEY_O))
    //{
    //    std::ofstream file("cpData.txt");

    //    if (!file.is_open())
    //    {
    //        LOG("ERROR: no se pudo crear cpData.txt");
    //    }
    //    else
    //    {
    //        file << "std::vector<CheckpointData> cpData = {\n";

    //        for (int i = 0; i < (int)savedPoints.size(); ++i)
    //        {
    //            const DebugPoint& p = savedPoints[i];

    //            int w = 300;
    //            int h = 120;

    //            float a = fmodf(fabsf(p.angle), 180.0f);
    //            if (a > 45.0f && a < 135.0f) { w = 120; h = 300; }

    //            file << "    {" << p.x << ", " << p.y << ", " << w << ", " << h << "}, // "
    //                << (i < 10 ? "0" : "") << i << "  A=" << p.angle << "\n";
    //        }

    //        file << "};\n";
    //        file.close();

    //        LOG("OK: cpData.txt generado. Copia y pega su contenido en Start().");
    //    }
    //}

    // Cámara offset para centrar el coche
    App->renderer->camera.x = (float)SCREEN_WIDTH * 0.5f - (float)carPx * MAP_SCALE;
    App->renderer->camera.y = (float)SCREEN_HEIGHT * 0.5f - (float)carPy * MAP_SCALE;

    float cam_x = App->renderer->camera.x;
    float cam_y = App->renderer->camera.y;

    // Dibuja el MAPA como mundo
    Vector2 mapPos = { cam_x, cam_y };
    DrawTextureEx(mapaMontmelo, mapPos, 0.0f, MAP_SCALE, WHITE);

    // brown rectangles
    DrawRectangle((int)(13360 + cam_x), (int)(5100 + cam_y), 300, 150, BROWN);
    DrawRectangle((int)(5800 + cam_x), (int)(4230 + cam_y), 300, 150, BROWN);

    int trackHeight = 60;
    int trackY = SCREEN_HEIGHT / 2 - trackHeight / 2;
    DrawRectangle((int)(80 + cam_x), (int)(trackY + cam_y), SCREEN_WIDTH - 160, trackHeight, GREEN);

    int startWidth = 40;
    DrawRectangle((int)(SCREEN_WIDTH / 2 - startWidth / 2 + cam_x), (int)(0 + cam_y),
        startWidth, SCREEN_HEIGHT, WHITE);

    // Actualizar entidades
    for (PhysicEntity* entity : entities)
        entity->Update();

    // ====================== HUD LEGIBLE ======================
    int hudX = 20, hudY = 20, hudW = 460, hudH = 300;
    DrawRectangle(hudX, hudY, hudW, hudH, Color{ 0, 0, 0, 190 });
    DrawRectangleLines(hudX, hudY, hudW, hudH, WHITE);

    int xHUD = hudX + 12;
    int yHUD = hudY + 10;
    int fsTitle = 22;
    int fs = 20;
    int line = 24;

    DrawText("RACE HUD", xHUD, yHUD, fsTitle, WHITE);
    yHUD += 30;

    // -------- Gasolina (arreglada) --------
    int gasPct = (int)((gasoline / (float)max_gasoline) * 100.0f);
    if (gasPct < 0) gasPct = 0;
    if (gasPct > 100) gasPct = 100;
    DrawText(TextFormat("Gasolina: %d%%", gasPct), xHUD, yHUD, fs, WHITE);
    yHUD += line;

    // -------- Velocidad (del player) --------
    float speedMS = 0.0f;
    if (car && car->body && car->body->body)
    {
        b2Vec2 v = car->body->body->GetLinearVelocity();
        speedMS = sqrtf(v.x * v.x + v.y * v.y);
    }
    float speedKMH = speedMS * 3.6f; // aproximado
    DrawText(TextFormat("Velocidad: %.1f km/h", speedKMH), xHUD, yHUD, fs, WHITE);
    yHUD += line;

    // -------- Crono vuelta player --------
    char curStr[32], bestStr[32], lastStr[32];
    FormatTime(sPlayerLapCurrent, curStr, 32);
    FormatTime((sPlayerLapBest >= 999998.0f) ? 0.0f : sPlayerLapBest, bestStr, 32);
    FormatTime(sPlayerLapLast, lastStr, 32);

    DrawText(TextFormat("Vuelta: %d/%d", lapCount, kMaxLaps), xHUD, yHUD, fs, WHITE);
    yHUD += line;

    DrawText(TextFormat("Lap:  %s", curStr), xHUD, yHUD, fs, WHITE);
    yHUD += line;

    DrawText(TextFormat("Best: %s", bestStr), xHUD, yHUD, fs, WHITE);
    yHUD += line;

    // -------- Top 3 mejores tiempos (IA) --------
    DrawText("TOP 3 IA (best lap)", xHUD, yHUD, fs, WHITE);
    yHUD += line;

    // recolectar (aiIndex, bestTime)
    struct AiRank { int idx; float t; };
    std::vector<AiRank> ranking;
    ranking.reserve(aiCars.size());

    for (int i = 0; i < (int)aiCars.size(); ++i)
    {
        float t = (i < (int)sAiLapBest.size()) ? sAiLapBest[i] : 999999.0f;
        if (t < 999998.0f) ranking.push_back({ i, t }); // solo si tienen tiempo válido
    }

    std::sort(ranking.begin(), ranking.end(), [](const AiRank& a, const AiRank& b) {
        return a.t < b.t;
        });

    for (int r = 0; r < 3; ++r)
    {
        if (r >= (int)ranking.size())
        {
            DrawText(TextFormat("%d) --:--.--", r + 1), xHUD, yHUD, fs, WHITE);
            yHUD += line;
            continue;
        }

        char tStr[32];
        FormatTime(ranking[r].t, tStr, 32);
        DrawText(TextFormat("%d) AI%02d  %s", r + 1, ranking[r].idx + 1, tStr), xHUD, yHUD, fs, WHITE);
        yHUD += line;
    }


    // ===== DEBUG: dibujar checkpoints =====
    for (int i = 0; i < (int)checkpoints.size(); ++i)
    {
        PhysBody* s = checkpoints[i];
        int x, y;
        s->GetPhysicPosition(x, y);

        float camX = App->renderer->camera.x;
        float camY = App->renderer->camera.y;

        Color c = (i == nextCheckpoint) ? YELLOW : RED;

        DrawRectangleLines(
            (int)(x + camX - s->width * 0.5f),
            (int)(y + camY - s->height * 0.5f),
            s->width,
            s->height,
            c
        );
    }

    //// ===== MOSTRAR LISTA DE PUNTOS GUARDADOS =====
    //int startY = 280;
    //DrawText("SAVED POINTS (press P):", 30, startY, 20, WHITE);

    //int lineY = startY + 30;
    //for (int i = (int)savedPoints.size() - 1; i >= 0; --i)
    //{
    //    const DebugPoint& pt = savedPoints[i];
    //    DrawText(TextFormat("%02d) X:%d Y:%d A:%.1f",
    //        i, pt.x, pt.y, pt.angle),
    //        30, lineY, 18, WHITE);

    //    lineY += 22;
    //    if (lineY > SCREEN_HEIGHT - 20) break;
    //}

    //// ===== DIST A SIGUIENTE CHECKPOINT (SAFE) =====
    //if (!checkpoints.empty())
    //{
    //    if (nextCheckpoint < 0 || nextCheckpoint >= (int)checkpoints.size())
    //        nextCheckpoint = 0;

    //    int cx = 0, cy = 0;
    //    checkpoints[nextCheckpoint]->GetPhysicPosition(cx, cy);

    //    float dist = sqrtf((float)((cx - carPx) * (cx - carPx) + (cy - carPy) * (cy - carPy)));
    //    DrawText(TextFormat("DIST: %.1f", dist), 30, 260, 20, WHITE);
    //}

    // ===== FIN A 3 VUELTAS =====
    // ===== LOSE: si la IA ganó, mostrar GAME OVER y cerrar tras delay =====
    if (sAiWon)
    {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Color{ 0, 0, 0, 220 });

        const char* msg = "GAME OVER";
        int fontSize = 120;
        int textW = MeasureText(msg, fontSize);
        DrawText(msg,
            (SCREEN_WIDTH - textW) / 2,
            (SCREEN_HEIGHT - fontSize) / 2,
            fontSize,
            WHITE);

        if (((float)GetTime() - sEndTime) >= kEndDelay)
            return UPDATE_STOP;

        return UPDATE_CONTINUE; // mientras se ve el GAME OVER no cierres aún
    }
    if (!sRaceFinished && lapCount >= kMaxLaps)
    {
        sRaceFinished = true;
        return UPDATE_STOP;
    }

    return UPDATE_CONTINUE;
}

void ModuleGame::OnCollision(PhysBody* bodyA, PhysBody* bodyB)
{
    if (checkpoints.empty() || car == nullptr) return;

    // ---------- PLAYER ----------
    if (bodyA == car->body || bodyB == car->body)
    {
        PhysBody* other = (bodyA == car->body) ? bodyB : bodyA;

        if (nextCheckpoint < 0 || nextCheckpoint >= (int)checkpoints.size())
            nextCheckpoint = 0;

        if (other == checkpoints[nextCheckpoint])
        {
            nextCheckpoint++;
            if (nextCheckpoint >= (int)checkpoints.size())
            {
                nextCheckpoint = 0;
                lapCount++;
                App->audio->PlayFx(bonus_fx);

                // ===== TIEMPOS PLAYER =====
                float now = (float)GetTime();
                sPlayerLapLast = now - sPlayerLapStart;
                if (sPlayerLapLast < sPlayerLapBest) sPlayerLapBest = sPlayerLapLast;
                sPlayerLapStart = now;
            }
        }
        return;
    }

    // ---------- IA (todas) ----------
    for (int i = 0; i < (int)aiCars.size(); ++i)
    {
        Box* ai = aiCars[i];
        if (ai == nullptr) continue;

        if (bodyA == ai->body || bodyB == ai->body)
        {
            PhysBody* other = (bodyA == ai->body) ? bodyB : bodyA;

            if (aiNextCP[i] < 0 || aiNextCP[i] >= (int)checkpoints.size())
                aiNextCP[i] = 0;

            if (other == checkpoints[aiNextCP[i]])
            {
                aiNextCP[i]++;
                if (aiNextCP[i] >= (int)checkpoints.size())
                {
                    aiNextCP[i] = 0;
                    aiLaps[i]++;   
                    if (!sAiWon && aiLaps[i] >= kMaxLaps)
                    {
                        sAiWon = true;
                        sEndTime = (float)GetTime();
                    }
                    // ===== TIEMPOS IA =====
                    if (i < (int)sAiLapStart.size())
                    {
                        float now = (float)GetTime();
                        sAiLapLast[i] = now - sAiLapStart[i];
                        if (sAiLapLast[i] < sAiLapBest[i]) sAiLapBest[i] = sAiLapLast[i];
                        sAiLapStart[i] = now;
                    }
                }
            }
            return;
        }
    }
}
