#include "Globals.h"
#include "Application.h"
#include "ModuleGame.h"
#include "ModuleAudio.h"
#include "ModulePhysics.h"
#include "ModuleRender.h"
#include <vector>

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

    const float acceleration = 0.0115f;
    const float braking = 0.020f;
    const float maxSpeed = 5.2f;

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
        {10779, 5460, 300, 120},// 0 salida/meta
        {7976, 42834, 200, 80}, // 1
        {15564, 43222, 200, 80}, // 2
        {16145, 43859, 200, 80}, // 3
        {16418, 44113, 200, 80}, // 4
 
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
    if (IsKeyPressed(KEY_P))  // P = guardar punto
    {
        DebugPoint pt;
        pt.x = carPx;
        pt.y = carPy;
        pt.angle = carAngle;

        savedPoints.push_back(pt);

        // Limitar a 20 puntos para no llenar la memoria
        if (savedPoints.size() > 20)
            savedPoints.erase(savedPoints.begin());

        LOG("SAVED POINT: X=%d Y=%d ANG=%.2f", pt.x, pt.y, pt.angle);
    }


    // Cámara offset para centrar el coche
    App->renderer->camera.x = (float)SCREEN_WIDTH * 0.5f - (float)carPx * MAP_SCALE;
    App->renderer->camera.y = (float)SCREEN_HEIGHT * 0.5f - (float)carPy * MAP_SCALE;

    float cam_x = App->renderer->camera.x;
    float cam_y = App->renderer->camera.y;

    // Dibuja el MAPA como mundo
    Vector2 mapPos = { cam_x, cam_y };
    DrawTextureEx(mapaMontmelo, mapPos, 0.0f, MAP_SCALE, WHITE);

    int trackHeight = 60;
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
    if (checkpoints.empty() || car == nullptr) return;

    // -------- PLAYER --------
    if (bodyA == car->body || bodyB == car->body)
    {
        PhysBody* other = (bodyA == car->body) ? bodyB : bodyA;

        // seguridad por si nextCheckpoint se va de rango
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
            }
        }
        return; // importante: para no procesar también IA en la misma colisión
    }

    // -------- IA --------
    if (aiCar != nullptr && (bodyA == aiCar->body || bodyB == aiCar->body))
    {
        PhysBody* other = (bodyA == aiCar->body) ? bodyB : bodyA;

        if (aiNextCheckpoint < 0 || aiNextCheckpoint >= (int)checkpoints.size())
            aiNextCheckpoint = 0;

        if (other == checkpoints[aiNextCheckpoint])
        {
            aiNextCheckpoint++;

            if (aiNextCheckpoint >= (int)checkpoints.size())
            {
                aiNextCheckpoint = 0;
                aiLapCount++;
            }
        }
        return;
    }
}
