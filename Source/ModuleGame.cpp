#include "Globals.h"
#include "Application.h"
#include "ModuleRender.h"
#include "ModuleGame.h"
#include "ModuleAudio.h"
#include "ModulePhysics.h"

#include <algorithm>

// Textura global para las ruedas delanteras (para no tocar el .h)
static Texture2D gFrontCarTexture;

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
// COCHE F1 (carrocería + ruedas delanteras separadas)
// =====================================================================
class Box : public PhysicEntity
{
public:
    Box(ModulePhysics* physics, int _x, int _y, Module* _listener,
        Texture2D bodyTex, Texture2D frontTex)
        : PhysicEntity(physics->CreateRectangle(_x, _y, 90, 40), _listener)
        , bodyTexture(bodyTex)
        , frontTexture(frontTex)
    {
    }

    void Update() override
    {
        HandleInput();
        UpdateMovement();
        DrawCar();
    }

private:

    // ---------------------- INPUT WASD ----------------------
    void HandleInput()
    {
        forwardInput = 0.0f;
        steeringInput = 0.0f;

        if (IsKeyDown(KEY_W)) forwardInput = 1.0f;
        if (IsKeyDown(KEY_S)) forwardInput = -1.0f;
        if (IsKeyDown(KEY_A)) steeringInput = -1.0f;
        if (IsKeyDown(KEY_D)) steeringInput = 1.0f;
    }

    // ---------------- VELOCIDAD / FRENADO / GIRO ----------------
    void UpdateMovement()
    {
        // Ángulo y posición actuales del cuerpo
        float angle = body->GetRotation();         // radianes
        b2Vec2 posMeters = body->body->GetPosition();

        // ---- ACELERAR / FRENAR ----
        if (forwardInput > 0.0f)                // W
        {
            speedCar += acceleration;
            if (speedCar > maxSpeed) speedCar = maxSpeed;
        }
        else if (forwardInput < 0.0f)           // S
        {
            speedCar -= acceleration;
            if (speedCar < -maxSpeed) speedCar = -maxSpeed;
        }
        else
        {
            // Frenado automático
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
        float speedFactor = fabsf(speedCar);       // cuanto más rápido, más gira
        float baseTurnSpeedRad = baseTurnSpeedDeg * DEGTORAD;

        angle += steeringInput * baseTurnSpeedRad * speedFactor;

        // ---- GIRO VISUAL DE LAS RUEDAS ----
        if (steeringInput != 0.0f)
        {
            steeringVisual += steeringInput * steerVisualSpeed;
            if (steeringVisual > maxSteerVisualDeg) steeringVisual = maxSteerVisualDeg;
            if (steeringVisual < -maxSteerVisualDeg) steeringVisual = -maxSteerVisualDeg;
        }
        else
        {
            // volver poco a poco al centro
            steeringVisual *= 0.85f;
        }

        // ---- APLICAR A BOX2D ----
        // Rotación
        body->body->SetTransform(posMeters, angle);

        // Velocidad alineada con el morro del coche
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

        float scale = 0.06f;

        // ---- CARROCERÍA ----
        Rectangle srcBody = { 0, 0, (float)bodyTexture.width, (float)bodyTexture.height };
        Rectangle dstBody = { (float)x, (float)y,
                              bodyTexture.width * scale,
                              bodyTexture.height * scale };
        Vector2 originBody = { dstBody.width / 2.0f, dstBody.height / 2.0f };

        DrawTexturePro(bodyTexture, srcBody, dstBody, originBody, angleDeg, WHITE);

        // ---- RUEDAS DELANTERAS ----
        float bodyW = dstBody.width;
        float bodyH = dstBody.height;

        // Offset desde el centro (ajústalo si hace falta)
        float forwardOffset = bodyW * 0.25f;   // hacia la punta del coche
        float halfTrack = bodyH * 0.28f;   // separación lateral de las ruedas

        float cosA = cosf(angle);
        float sinA = sinf(angle);

        // Rueda delantera derecha (desde nuestro punto de vista superior)
        Vector2 wheelRight;
        wheelRight.x = x + cosA * forwardOffset - sinA * (-halfTrack);
        wheelRight.y = y + sinA * forwardOffset + cosA * (-halfTrack);

        // Rueda delantera izquierda
        Vector2 wheelLeft;
        wheelLeft.x = x + cosA * forwardOffset - sinA * (halfTrack);
        wheelLeft.y = y + sinA * forwardOffset + cosA * (halfTrack);

        Rectangle srcWheel = { 0, 0, (float)frontTexture.width, (float)frontTexture.height };
        Rectangle dstWheel = { 0, 0,
                               frontTexture.width * scale,
                               frontTexture.height * scale };
        Vector2 originWheel = { dstWheel.width / 2.0f, dstWheel.height / 2.0f };

        float wheelAngleDeg = angleDeg + steeringVisual; // ruedas giran más

        // derecha
        dstWheel.x = wheelRight.x;
        dstWheel.y = wheelRight.y;
        DrawTexturePro(frontTexture, srcWheel, dstWheel, originWheel, wheelAngleDeg, WHITE);

        // izquierda
        dstWheel.x = wheelLeft.x;
        dstWheel.y = wheelLeft.y;
        DrawTexturePro(frontTexture, srcWheel, dstWheel, originWheel, wheelAngleDeg, WHITE);
    }

private:
    Texture2D bodyTexture;
    Texture2D frontTexture;

    // Inputs
    float forwardInput = 0.0f;   // W/S
    float steeringInput = 0.0f;   // A/D

    // Movimiento
    float speedCar = 0.0f;
    float steeringVisual = 0.0f;  // sólo gráfico (ruedas)

    // Parámetros de movimiento
    const float acceleration = 0.005f;
    const float braking = 0.05f;
    const float maxSpeed = 2.0f;
    const float moveFactor = 4.0f;   // escala para velocidad en Box2D
    const float baseTurnSpeedDeg = 2.0f;   // grados de giro base
    const float maxSteerVisualDeg = 25.0f;  // giro máximo de ruedas
    const float steerVisualSpeed = 3.0f;   // rapidez con la que giran visualmente
};

// =====================================================================
// MODULE GAME
// =====================================================================

ModuleGame::ModuleGame(Application* app, bool start_enabled) : Module(app, start_enabled)
{
    ray_on = false;
    sensed = false;
}

ModuleGame::~ModuleGame()
{
}

// Load assets
bool ModuleGame::Start()
{
    LOG("Loading Game assets");
    bool ret = true;

    App->renderer->camera.x = App->renderer->camera.y = 0;

    // Texturas del coche
    carTexture = LoadTexture("Assets/f1_body_car.png");
    gFrontCarTexture = LoadTexture("Assets/f1_front_car.png");

    if (carTexture.id == 0)
        LOG("No se ha podido cargar Assets/f1_body_car.png");
    if (gFrontCarTexture.id == 0)
        LOG("No se ha podido cargar Assets/f1_front_car.png");

    bonus_fx = App->audio->LoadFx("Assets/bonus.wav");

    entities.clear();

    // Crear el coche en el centro de la pantalla
    car = new Box(App->physics,
        SCREEN_WIDTH / 2,
        SCREEN_HEIGHT / 2,
        this,
        carTexture,
        gFrontCarTexture);

    entities.emplace_back(car);

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

    return true;
}

// Update: draw background + entities
update_status ModuleGame::Update()
{
    // Fondo
    ClearBackground(BLACK);

    // ----- Circuito sencillo (puedes cambiarlo luego) -----
    // Bordes rojos
    DrawRectangle(0, 0, SCREEN_WIDTH, 20, RED);
    DrawRectangle(0, SCREEN_HEIGHT - 20, SCREEN_WIDTH, 20, RED);
    DrawRectangle(0, 0, 20, SCREEN_HEIGHT, RED);
    DrawRectangle(SCREEN_WIDTH - 20, 0, 20, SCREEN_HEIGHT, RED);

    // Recta verde central
    int trackHeight = 60;
    int trackY = SCREEN_HEIGHT / 2 - trackHeight / 2;
    DrawRectangle(80, trackY, SCREEN_WIDTH - 160, trackHeight, GREEN);

    // Línea de salida
    int startWidth = 40;
    DrawRectangle(SCREEN_WIDTH / 2 - startWidth / 2, 0,
        startWidth, SCREEN_HEIGHT, WHITE);

    // ----- Actualizar / dibujar entidades (coche) -----
    for (PhysicEntity* entity : entities)
        entity->Update();

    return UPDATE_CONTINUE;
}

// Por ahora no usamos colisiones
void ModuleGame::OnCollision(PhysBody* bodyA, PhysBody* bodyB)
{
    // Aquí puedes gestionar colisiones más adelante
}
