#include "Globals.h"
#include "Application.h"
#include "ModuleGame.h"
#include "ModuleAudio.h"
#include "ModulePhysics.h"
#include "ModuleRender.h"
#include <vector>
#include <fstream>

#include "raylib.h"
#include <cmath>

static Texture2D gFrontCarTexture;
static Texture2D gFrontCarTextureLeft;
static Texture2D gFrontCarTextureRight;

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
        Texture2D bodyTex, Texture2D frontTex, bool ai = false)
        : PhysicEntity(physics->CreateRectangle(_x, _y, 90, 40), _listener)
        , bodyTexture(bodyTex)
        , frontTexture(frontTex)
        , isAI(ai)
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
    // ---------------------- INPUT WASD (JUGADOR) ----------------------
    void HandleInput()
    {
        forwardInput = 0.0f;
        steeringInput = 0.0f;

        if (IsKeyDown(KEY_W)) forwardInput = 1.0f;
        if (IsKeyDown(KEY_S)) forwardInput = -1.0f;
        if (IsKeyDown(KEY_A)) steeringInput = -1.0f;
     if (IsKeyDown(KEY_D)) steeringInput = 1.0f;
    }

    // ---------------------- IA SIMPLE: seguir checkP1->checkP2->checkP3 ----------------------
    void UpdateAI()
    {
        forwardInput = 1.0f;   // siempre acelera
        steeringInput = 0.0f;

        int x, y;
        body->GetPhysicPosition(x, y);

        ModuleGame* game = (ModuleGame*)listener;

        PhysBody* target = nullptr;
        if (!game->checkpoints.empty())
        {
            int idx = game->aiNextCheckpoint;
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

        // giro simple (estilo estudiante)
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

 const int brown1_x =13360;
 const int brown1_y =5100;
 const int brown2_x =5800;
 const int brown2_y =4230;
 const int brown_w =300;
 const int brown_h =150;

 bool insideBrown = false;
 if (car_px >= brown1_x && car_px <= brown1_x + brown_w && car_py >= brown1_y && car_py <= brown1_y + brown_h)
 insideBrown = true;
 else if (car_px >= brown2_x && car_px <= brown2_x + brown_w && car_py >= brown2_y && car_py <= brown2_y + brown_h)
 insideBrown = true;

 // Car must be inside rectangle AND be stationary to regenerate
 const float stop_threshold =0.05f; // small speed threshold
 if (insideBrown && fabsf(speedCar) < stop_threshold && forwardInput ==0.0f)
 {
 game->gasoline +=20.0f * dt; //20 units per second
 if (game->gasoline > (float)game->max_gasoline) game->gasoline = (float)game->max_gasoline;
 }
 }

 // Debug: only for player car
 if (!isAI)
 {
 LOG("UpdateMovement (player): forwardInput=%.2f speed=%.2f gasoline=%.3f dt=%.4f", forwardInput, speedCar, game->gasoline, dt);
 }

 // ---- ACELERAR / FRENAR ----
 // Only allow acceleration if gasoline >0 (for player)
 bool canAccelerate = true;
 if (!isAI) canAccelerate = (game->gasoline >0.0f);

 bool inputMoving = (forwardInput >0.0f || forwardInput <0.0f);

 if (isAI)
 {
 // AI: move without consuming player's gasoline
 if (forwardInput >0.0f)
 {
 speedCar += acceleration;
 if (speedCar > maxSpeed) speedCar = maxSpeed;
 }
 else if (forwardInput <0.0f)
 {
 speedCar -= acceleration;
 if (speedCar < -maxSpeed) speedCar = -maxSpeed;
 }
 }
 else
 {
 // Player: only accelerate if has gasoline
 if (inputMoving && canAccelerate)
 {
 if (forwardInput >0.0f)
 {
 speedCar += acceleration;
 if (speedCar > maxSpeed) speedCar = maxSpeed;
 }
 else if (forwardInput <0.0f)
 {
 speedCar -= acceleration;
 if (speedCar < -maxSpeed) speedCar = -maxSpeed;
 }

 // Drain gasoline while player is pressing throttle (framerate-independent)
 game->gasoline -= game->gasoline_drain_rate * dt;
 if (game->gasoline <0.0f) game->gasoline =0.0f;
 }
 else
 {
 // No input movement: apply friction/braking for player
 if (speedCar >0.0f)
 {
 speedCar -= braking;
 if (speedCar <0.0f) speedCar =0.0f;
 }
 else if (speedCar <0.0f)
 {
 speedCar += braking;
 if (speedCar >0.0f) speedCar =0.0f;
 }
 }
 }

    // ---- GIRO DEL COCHE ----
    float speedFactor = fabsf(speedCar);
    float baseTurnSpeedRad = baseTurnSpeedDeg * DEGTORAD;

    angle += steeringInput * baseTurnSpeedRad * speedFactor;

    // ---- GIRO VISUAL (opcional) ----
    if (steeringInput !=0.0f)
    {
        steeringVisual += steeringInput * steerVisualSpeed;
        if (steeringVisual > maxSteerVisualDeg) steeringVisual = maxSteerVisualDeg;
        if (steeringVisual < -maxSteerVisualDeg) steeringVisual = -maxSteerVisualDeg;
    }
    else
    {
        steeringVisual *=0.85f;
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
    const float maxSpeed = 10.0f;

    const float moveFactor = 2.0f;

    const float baseTurnSpeedDeg = 0.35f;

    const float maxSteerVisualDeg = 12.0f;
    const float steerVisualSpeed = 1.5f;

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

    // IA
    aiCar = nullptr;
    aiCar2 = nullptr;
    aiCar3 = nullptr;
    aiCar4 = nullptr;
    aiCar5 = nullptr;
    aiCar6 = nullptr;
    aiCar7 = nullptr;
    aiCar8 = nullptr;
    aiCar9 = nullptr;
    aiCar10 = nullptr;
    aiCar11 = nullptr;

    aiLapCount = 0;
    aiNextCheckpoint = 1;

}

ModuleGame::~ModuleGame()
{
}

bool ModuleGame::Start()
{

    LOG("Loading Game assets");
    bool ret = true;

    App->renderer->camera.x = App->renderer->camera.y = 0;

    //mapa
    mapaMontmelo = LoadTexture("Assets/mapa_montmelo.png");

    // Texturas del coche
    carTexture = LoadTexture("Assets/f1_body_car.png");
    gFrontCarTexture = LoadTexture("Assets/f1_front_car.png");
    gFrontCarTextureLeft = LoadTexture("Assets/f1_front_car_Left.png");
    gFrontCarTextureRight = LoadTexture("Assets/f1_front_car_Right.png");

    bonus_fx = App->audio->LoadFx("Assets/bonus.wav");

    entities.clear();

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

    // Coche IA
    aiCar = new Box(App->physics,
        SCREEN_WIDTH / 2 + 120,
        SCREEN_HEIGHT / 2,
        this,
        carTexture,
        gFrontCarTexture,
        true);
    
    entities.emplace_back(aiCar);

    car->body->body->SetFixedRotation(true);
    aiCar->body->body->SetFixedRotation(true); 

// ================= CHECKPOINTS =================
    checkpoints.clear();

    struct CheckpointData
    {
        int x, y, w, h;
    };

    std::vector<CheckpointData> cpData =
    {
    {10779, 5460, 300, 120}, // 00  A=180
    {9733, 5384, 300, 120}, // 01  A=181.05
    {8119, 5381, 300, 120}, // 02  A=177.55
    {6889, 5378, 300, 120}, // 03  A=184.55
    {5792, 5404, 300, 120}, // 04  A=181.05
    {4111, 5370, 300, 120}, // 05  A=181.05
    {3463, 5348, 300, 120}, // 06  A=184.55
    {3244, 5293, 300, 120}, // 07  A=208.399
    {3117, 5174, 120, 300}, // 08  A=231.765
    {3041, 5057, 120, 300}, // 09  A=244.778
    {2998, 4859, 120, 300}, // 10  A=267.944
    {2997, 4559, 120, 300}, // 11  A=260.944
    {2953, 4400, 120, 300}, // 12  A=246.944
    {2858, 4244, 120, 300}, // 13  A=232.944
    {2668, 4082, 300, 120}, // 14  A=218.944
    {2407, 3937, 300, 120}, // 15  A=204.944
    {2110, 3829, 300, 120}, // 16  A=197.944
    {1922, 3761, 300, 120}, // 17  A=208.444
    {1658, 3618, 300, 120}, // 18  A=208.444
    {1454, 3476, 300, 120}, // 19  A=218.944
    {1302, 3323, 120, 300}, // 20  A=229.444
    {1208, 3166, 120, 300}, // 21  A=239.944
    {1131, 2947, 120, 300}, // 22  A=260.944
    {1116, 2714, 120, 300}, // 23  A=267.944
    {1155, 2367, 120, 300}, // 24  A=278.444
    {1251, 2103, 120, 300}, // 25  A=302.944
    {1449, 1836, 120, 300}, // 26  A=309.944
    {1752, 1529, 300, 120}, // 27  A=323.945
    {2111, 1288, 300, 120}, // 28  A=330.945
    {2405, 1174, 300, 120}, // 29  A=341.445
    {2679, 1102, 300, 120}, // 30  A=348.445
    {2944, 1079, 300, 120}, // 31  A=358.945
    {3327, 1072, 300, 120}, // 32  A=358.945
    {3910, 1061, 300, 120}, // 33  A=358.945
    {4375, 1080, 300, 120}, // 34  A=358.945
    {5275, 1063, 300, 120}, // 35  A=358.945
    {5604, 1096, 300, 120}, // 36  A=372.945
    {5820, 1183, 300, 120}, // 37  A=393.945
    {6013, 1390, 120, 300}, // 38  A=411.445
    {6087, 1499, 120, 300}, // 39  A=425.445
    {6154, 1669, 120, 300}, // 40  A=439.445
    {6175, 1818, 120, 300}, // 41  A=449.945
    {6139, 1997, 120, 300}, // 42  A=470.945
    {6057, 2141, 120, 300}, // 43  A=488.445
    {5953, 2272, 120, 300}, // 44  A=488.445
    {5810, 2410, 300, 120}, // 45  A=502.445
    {5653, 2504, 300, 120}, // 46  A=512.945
    {5437, 2590, 300, 120}, // 47  A=523.445
    {5240, 2617, 300, 120}, // 48  A=537.445
    {4990, 2628, 300, 120}, // 49  A=537.445
    {4524, 2649, 300, 120}, // 50  A=537.445
    {3892, 2657, 300, 120}, // 51  A=537.445
    {3276, 2684, 300, 120}, // 52  A=537.445
    {2993, 2703, 300, 120}, // 53  A=526.945
    {2890, 2756, 300, 120}, // 54  A=502.445
    {2805, 2877, 120, 300}, // 55  A=470.945
    {2797, 3025, 120, 300}, // 56  A=439.445
    {2837, 3238, 120, 300}, // 57  A=435.945
    {2927, 3375, 300, 120}, // 58  A=400.945
    {3102, 3471, 300, 120}, // 59  A=386.945
    {3367, 3643, 300, 120}, // 60  A=397.445
    {3651, 3846, 300, 120}, // 61  A=390.445
    {3822, 3950, 300, 120}, // 62  A=393.945
    {3974, 4053, 300, 120}, // 63  A=393.945
    {4114, 4143, 300, 120}, // 64  A=386.945
    {4337, 4256, 300, 120}, // 65  A=386.945
    {4503, 4334, 300, 120}, // 66  A=376.445
    {4678, 4386, 300, 120}, // 67  A=376.445
    {4854, 4437, 300, 120}, // 68  A=376.445
    {5083, 4474, 300, 120}, // 69  A=362.445
    {5316, 4484, 300, 120}, // 70  A=362.445
    {5550, 4493, 300, 120}, // 71  A=362.445
    {5782, 4490, 300, 120}, // 72  A=355.445
    {5982, 4493, 300, 120}, // 73  A=365.945
    {6198, 4500, 300, 120}, // 74  A=358.945
    {6397, 4490, 300, 120}, // 75  A=348.445
    {6555, 4437, 300, 120}, // 76  A=330.944
    {6664, 4315, 120, 300}, // 77  A=295.944
    {6708, 4189, 120, 300}, // 78  A=288.944
    {6743, 4061, 120, 300}, // 79  A=278.444
    {6760, 3895, 120, 300}, // 80  A=271.444
    {6766, 3645, 120, 300}, // 81  A=271.444
    {6808, 3484, 120, 300}, // 82  A=292.444
    {6897, 3324, 120, 300}, // 83  A=299.444
    {7051, 3087, 120, 300}, // 84  A=306.444
    {7189, 2899, 120, 300}, // 85  A=306.444
    {7298, 2752, 120, 300}, // 86  A=306.444
    {7437, 2564, 120, 300}, // 87  A=306.444
    {7565, 2390, 120, 300}, // 88  A=306.444
    {7694, 2215, 120, 300}, // 89  A=306.444
    {7861, 1967, 120, 300}, // 90  A=299.444
    {8036, 1704, 120, 300}, // 91  A=309.944
    {8186, 1525, 120, 300}, // 92  A=309.944
    {8323, 1404, 300, 120}, // 93  A=327.444
    {8477, 1341, 300, 120}, // 94  A=344.945
    {8658, 1311, 300, 120}, // 95  A=358.945
    {8890, 1328, 300, 120}, // 96  A=369.445
    {9176, 1410, 300, 120}, // 97  A=383.445
    {9375, 1497, 300, 120}, // 98  A=386.945
    {9547, 1599, 300, 120}, // 99  A=390.445
    {9791, 1742, 300, 120}, // 100  A=390.445
    {9949, 1835, 300, 120}, // 101  A=390.445
    {10150, 1953, 300, 120}, // 102  A=390.445
    {10366, 2080, 300, 120}, // 103  A=390.445
    {10653, 2249, 300, 120}, // 104  A=390.445
    {10883, 2384, 300, 120}, // 105  A=390.445
    {11070, 2494, 300, 120}, // 106  A=390.445
    {11314, 2637, 300, 120}, // 107  A=390.445
    {11530, 2764, 300, 120}, // 108  A=390.445
    {11760, 2899, 300, 120}, // 109  A=390.445
    {12018, 3051, 300, 120}, // 110  A=390.445
    {12263, 3195, 300, 120}, // 111  A=390.445
    {12550, 3364, 300, 120}, // 112  A=390.445
    {12809, 3516, 300, 120}, // 113  A=390.445
    {13125, 3701, 300, 120}, // 114  A=390.445
    {13455, 3896, 300, 120}, // 115  A=390.445
    {13700, 4039, 300, 120}, // 116  A=390.445
    {13940, 4148, 300, 120}, // 117  A=366.015
    {14052, 4133, 300, 120}, // 118  A=341.907
    {13969, 3436, 120, 300}, // 119  A=238.93
    {13854, 3172, 120, 300}, // 120  A=247.939
    {13711, 2914, 120, 300}, // 121  A=237.475
    {13591, 2755, 300, 120}, // 122  A=219.975
    {13400, 2655, 300, 120}, // 123  A=205.975
    {13190, 2553, 300, 120}, // 124  A=205.975
    {12998, 2453, 300, 120}, // 125  A=216.475
    {12864, 2309, 120, 300}, // 126  A=244.475
    {12811, 2099, 120, 300}, // 127  A=265.475
    {12833, 1885, 120, 300}, // 128  A=282.976
    {12910, 1648, 120, 300}, // 129  A=300.476
    {13036, 1516, 300, 120}, // 130  A=321.476
    {13251, 1391, 300, 120}, // 131  A=345.976
    {13483, 1372, 300, 120}, // 132  A=366.976
    {13707, 1432, 300, 120}, // 133  A=380.976
    {14003, 1545, 300, 120}, // 134  A=380.976
    {14454, 1718, 300, 120}, // 135  A=380.976
    {14863, 1859, 300, 120}, // 136  A=373.976
    {15190, 1981, 300, 120}, // 137  A=384.476
    {15463, 2136, 120, 300}, // 138  A=408.976
    {15602, 2401, 120, 300}, // 139  A=429.976
    {15648, 2679, 120, 300}, // 140  A=447.476
    {15651, 3063, 120, 300}, // 141  A=450.976
    {15641, 3646, 120, 300}, // 142  A=450.976
    {15630, 4296, 120, 300}, // 143  A=450.976
    {15619, 4595, 120, 300}, // 144  A=461.476
    {15535, 4810, 120, 300}, // 145  A=482.476
    {15419, 4993, 120, 300}, // 146  A=482.476
    {15262, 5165, 300, 120}, // 147  A=496.476
    {15082, 5285, 300, 120}, // 148  A=506.976
    {14879, 5362, 300, 120}, // 149  A=520.976
    {14568, 5405, 300, 120}, // 150  A=541.976
    {13952, 5391, 300, 120}, // 151  A=545.476
    {13636, 5394, 300, 120}, // 152  A=538.476
    {12838, 5382, 300, 120}, // 153  A=541.976
    {12205, 5379, 300, 120}, // 154  A=538.476
    };


    for (const auto& cp : cpData)
    {
        PhysBody* sensor = App->physics->CreateRectangleSensor(
            cp.x,
            cp.y,
            cp.w,
            cp.h
        );

        sensor->listener = this;
        checkpoints.push_back(sensor);
    }

    // Reset contadores
    lapCount = 0;
    nextCheckpoint = 0;
    aiLapCount = 0;
    aiNextCheckpoint = 0;

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
//1

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

    // ===== GUARDAR COORDENADAS CON TECLA =====
    // P = guardar punto
   // P = guardar punto
    if (IsKeyPressed(KEY_P))
    {
        DebugPoint pt;
        pt.x = carPx;
        pt.y = carPy;
        pt.angle = carAngle;

        savedPoints.push_back(pt);
        LOG("SAVED: X=%d Y=%d A=%.1f (total=%d)", pt.x, pt.y, pt.angle, (int)savedPoints.size());
    }

    // O = exportar a archivo cpData.txt
    if (IsKeyPressed(KEY_O))
    {
        std::ofstream file("cpData.txt");

        if (!file.is_open())
        {
            LOG("ERROR: no se pudo crear cpData.txt");
        }
        else
        {
            file << "std::vector<CheckpointData> cpData = {\n";

            for (int i = 0; i < (int)savedPoints.size(); ++i)
            {
                const DebugPoint& p = savedPoints[i];

                int w = 300;
                int h = 120;

                float a = fmodf(fabsf(p.angle), 180.0f);
                if (a > 45.0f && a < 135.0f) { w = 120; h = 300; }

                file << "    {" << p.x << ", " << p.y << ", " << w << ", " << h << "}, // "
                    << (i < 10 ? "0" : "") << i << "  A=" << p.angle << "\n";
            }

            file << "};\n";
            file.close();

            LOG("OK: cpData.txt generado. Copia y pega su contenido en Start().");
        }
    }




    // Cámara offset para centrar el coche
    App->renderer->camera.x = (float)SCREEN_WIDTH * 0.5f - (float)carPx * MAP_SCALE;
    App->renderer->camera.y = (float)SCREEN_HEIGHT * 0.5f - (float)carPy * MAP_SCALE;

    float cam_x = App->renderer->camera.x;
    float cam_y = App->renderer->camera.y;

    // Dibuja el MAPA como mundo
    Vector2 mapPos = { cam_x, cam_y };
    DrawTextureEx(mapaMontmelo, mapPos,0.0f, MAP_SCALE, WHITE);

    // brown rectangle moved and resized: at (13360,5150), width=300, height=150
    // DrawRectangle((int)(13360 + cam_x), (int)(5150 + cam_y),300,150, BROWN);
    // brown rectangle moved and resized: at (13360,5100), width=300, height=150
    DrawRectangle((int)(13360 + cam_x), (int)(5100 + cam_y),300,150, BROWN);
    DrawRectangle((int)(5800 + cam_x), (int)(4230 + cam_y),300,150, BROWN);

    int trackHeight =60;
    int trackY = SCREEN_HEIGHT / 2 - trackHeight / 2;
    DrawRectangle((int)(80 + cam_x), (int)(trackY + cam_y), SCREEN_WIDTH - 160, trackHeight, GREEN);

    int startWidth = 40;
    DrawRectangle((int)(SCREEN_WIDTH / 2 - startWidth / 2 + cam_x), (int)(0 + cam_y),
        startWidth, SCREEN_HEIGHT, WHITE);

    // Actualizar entidades
    for (PhysicEntity* entity : entities)
        entity->Update();

    // HUD
    DrawText(TextFormat("PLAYER CP: %d/%d", nextCheckpoint, (int)checkpoints.size()), 30, 70, 20, WHITE);
    DrawText(TextFormat("AI CP: %d/%d", aiNextCheckpoint, (int)checkpoints.size()), 30, 140, 20, WHITE);
	DrawText(TextFormat("LAPS: %d", lapCount), 30, 100, 20, WHITE);
	DrawText(TextFormat("AI LAPS: %d", aiLapCount), 30, 120, 20, WHITE);

    // Gasoline meter
    DrawText(TextFormat("Gasolina: %d/100", (int)gasoline), 30, 40, 20, WHITE);



    DrawText(TextFormat("CAR X: %d", carPx), 30, 180, 20, WHITE);
    DrawText(TextFormat("CAR Y: %d", carPy), 30, 210, 20, WHITE);
    DrawText(TextFormat("CAR ANGLE: %.2f", carAngle), 30, 240, 20, WHITE);

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

    // ===== MOSTRAR LISTA DE PUNTOS GUARDADOS =====
    int startY = 280;
    DrawText("SAVED POINTS (press P):", 30, startY, 20, WHITE);

    int lineY = startY + 30;
    for (int i = (int)savedPoints.size() - 1; i >= 0; --i)
    {
        const DebugPoint& pt = savedPoints[i];
        DrawText(TextFormat("%02d) X:%d Y:%d A:%.1f",
            i, pt.x, pt.y, pt.angle),
            30, lineY, 18, WHITE);

        lineY += 22;
        if (lineY > SCREEN_HEIGHT - 20) break; // no bajar infinito
    }

    int cx, cy; checkpoints[nextCheckpoint]->GetPhysicPosition(cx, cy);
    DrawText(TextFormat("DIST: %.1f", sqrtf((cx - carPx) * (cx - carPx) + (cy - carPy) * (cy - carPy))), 30, 260, 20, WHITE);


    return UPDATE_CONTINUE;
}


void ModuleGame::OnCollision(PhysBody* bodyA, PhysBody* bodyB)
{
    if (checkpoints.empty()) return;

    // -------- PLAYER --------
    if (bodyA == car->body || bodyB == car->body)
    {
        PhysBody* other = (bodyA == car->body) ? bodyB : bodyA;

        if (other == checkpoints[nextCheckpoint])
        {
            nextCheckpoint++;

            if (nextCheckpoint >= (int)checkpoints.size())
            {
                nextCheckpoint = 0;
                lapCount++;
                App->audio->PlayFx(bonus_fx);
            }
        }
        return;
    }

    // -------- IA --------
    if (aiCar && (bodyA == aiCar->body || bodyB == aiCar->body))
    {
        PhysBody* other = (bodyA == aiCar->body) ? bodyB : bodyA;

        if (other == checkpoints[aiNextCheckpoint])
        {
            aiNextCheckpoint++;

            if (aiNextCheckpoint >= (int)checkpoints.size())
            {
                aiNextCheckpoint = 0;
                aiLapCount++;
            }
        }
    }
}

