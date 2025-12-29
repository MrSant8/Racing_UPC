#include "Globals.h"
#include "Application.h"
#include "ModuleGame.h"
#include "ModuleAudio.h"
#include "ModulePhysics.h"
#include "ModuleRender.h"

#include "raylib.h"
#include <cmath>

// Texturas globales del morro (para no tocar el .h)
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
        if (game->aiNextCheckpoint == 1) target = game->checkP1;
        else if (game->aiNextCheckpoint == 2) target = game->checkP2;
        else target = game->checkP3;

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
        float angle = body->GetRotation();         // radianes
        b2Vec2 posMeters = body->body->GetPosition();

        // ---- ACELERAR / FRENAR ----
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

        // ---- GIRO DEL COCHE ----
        float speedFactor = fabsf(speedCar);
        float baseTurnSpeedRad = baseTurnSpeedDeg * DEGTORAD;

        angle += steeringInput * baseTurnSpeedRad * speedFactor;

        // ---- GIRO VISUAL (opcional) ----
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

        float scale = 0.05f;

        // ===== CARROCERÍA =====
        Rectangle srcBody = { 0, 0, (float)bodyTexture.width, (float)bodyTexture.height };
        Rectangle dstBody = {
            (float)x, (float)y,
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
        frontPos.x = x + cosA * forwardOffset;
        frontPos.y = y + sinA * forwardOffset;

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

    const float acceleration = 0.005f;
    const float braking = 0.05f;
    const float maxSpeed = 0.5f;
    const float moveFactor = 4.0f;
    const float baseTurnSpeedDeg = 2.0f;
    const float maxSteerVisualDeg = 15.0f;
    const float steerVisualSpeed = 3.0f;
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

    checkP1 = checkP2 = checkP3 = nullptr;
}

ModuleGame::~ModuleGame()
{
}

bool ModuleGame::Start()
{

    LOG("Loading Game assets");
    bool ret = true;

    App->renderer->camera.x = App->renderer->camera.y = 0;

    // (Opcional) mapa, por ahora no lo dibujas
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
        SCREEN_WIDTH / 2,
        SCREEN_HEIGHT / 2,
        this,
        carTexture,
        gFrontCarTexture,
        false);

    entities.emplace_back(car);

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

    // Checkpoints (los tuyos)
    // X Y ANCHURA ALTURA
    checkP1 = App->physics->CreateRectangleSensor(300, 300, 80, 40);  checkP1->listener = this;
    checkP2 = App->physics->CreateRectangleSensor(800, 300, 80, 40);  checkP2->listener = this;
    checkP3 = App->physics->CreateRectangleSensor(800, 600, 80, 40);  checkP3->listener = this;

    // Reset contadores
    lapCount = 0;
    nextCheckpoint = 1;
    aiLapCount = 0;
    aiNextCheckpoint = 1;

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
    // Update camera to follow player car
    if (car != nullptr)
    {
        int carPx, carPy;
        car->body->GetPhysicPosition(carPx, carPy);

        // Camera follows player, keeping them centered on screen
        // MAP_SCALE determines the zoom level (higher = more zoomed in)
        constexpr float MAP_SCALE = 4.0f;
        App->renderer->camera.x = (float)SCREEN_WIDTH * 0.5f - (float)carPx;
        App->renderer->camera.y = (float)SCREEN_HEIGHT * 0.5f - (float)carPy;
    }

    // Draw map background at fixed screen position (no camera offset)
    Rectangle map_src = { 0, 0, (float)mapaMontmelo.width, (float)mapaMontmelo.height };
    Rectangle map_dst = { 0, 0, (float)mapaMontmelo.width, (float)mapaMontmelo.height };
    DrawTexturePro(mapaMontmelo, map_src, map_dst, { 0, 0 }, 0.0f, WHITE);

    float cam_x = App->renderer->camera.x;
    float cam_y = App->renderer->camera.y;

    // Circuito simple - apply camera offset to world objects
    DrawRectangle((int)(0 + cam_x), (int)(0 + cam_y), SCREEN_WIDTH, 20, RED);
    DrawRectangle((int)(0 + cam_x), (int)(SCREEN_HEIGHT - 20 + cam_y), SCREEN_WIDTH, 20, RED);
    DrawRectangle((int)(0 + cam_x), (int)(0 + cam_y), 20, SCREEN_HEIGHT, RED);
    DrawRectangle((int)(SCREEN_WIDTH - 20 + cam_x), (int)(0 + cam_y), 20, SCREEN_HEIGHT, RED);

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
    DrawText(TextFormat("PLAYER LAPS: %d", lapCount), 30, 40, 20, BLACK);
    DrawText(TextFormat("PLAYER CP: %d/3", nextCheckpoint - 1), 30, 70, 20, BLACK);

    DrawText(TextFormat("AI LAPS: %d", aiLapCount), 30, 110, 20, BLACK);
    DrawText(TextFormat("AI CP: %d/3", aiNextCheckpoint - 1), 30, 140, 20, BLACK);

    return UPDATE_CONTINUE;
}

void ModuleGame::OnCollision(PhysBody* bodyA, PhysBody* bodyB)
{
    PhysBody* other = nullptr;
    bool hitPlayer = false;
    bool hitAI = false;

    // ¿Colisiona jugador con sensor?
    if (bodyA == car->body) { other = bodyB; hitPlayer = true; }
    else if (bodyB == car->body) { other = bodyA; hitPlayer = true; }
    // ¿Colisiona IA con sensor?
    else if (aiCar && bodyA == aiCar->body) { other = bodyB; hitAI = true; }
    else if (aiCar && bodyB == aiCar->body) { other = bodyA; hitAI = true; }
    else return;

    // -------- PLAYER --------
    if (hitPlayer)
    {
        if (nextCheckpoint == 1 && other == checkP1) nextCheckpoint = 2;
        else if (nextCheckpoint == 2 && other == checkP2) nextCheckpoint = 3;
        else if (nextCheckpoint == 3 && other == checkP3)
        {
            lapCount++;
            nextCheckpoint = 1;
            App->audio->PlayFx(bonus_fx);
        }
    }

    // -------- IA --------
    if (hitAI)
    {
        if (aiNextCheckpoint == 1 && other == checkP1) aiNextCheckpoint = 2;
        else if (aiNextCheckpoint == 2 && other == checkP2) aiNextCheckpoint = 3;
        else if (aiNextCheckpoint == 3 && other == checkP3)
        {
            aiLapCount++;
            aiNextCheckpoint = 1;
        }
    }
}
