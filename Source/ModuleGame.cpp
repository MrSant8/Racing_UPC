#include "Globals.h"
#include "Application.h"
#include "ModuleRender.h"
#include "ModuleGame.h"
#include "ModuleAudio.h"
#include "ModulePhysics.h"
#include "ModuleRender.h"   
#include "ModuleWindow.h"

#include <algorithm>

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
    //void DrawCar()
    //{
    //    int x, y;
    //    body->GetPhysicPosition(x, y);
    //    float angle = body->GetRotation();

    //    // **********************************************
    //    // * CORRECCIÓN: Declaración de variables *
    //    // **********************************************
    //    float angleDeg = angle * RAD2DEG; // <-- Identificador no declarado
    //    float scale = 0.05f;             // <-- Identificador no declarado

    //    // ---- CARROCERÍA ----
    //    Rectangle srcBody = { 0, 0, (float)bodyTexture.width, (float)bodyTexture.height };
    //    Rectangle dstBody = { (float)x, (float)y,
    //                          bodyTexture.width * scale,
    //                          bodyTexture.height * scale };
    //    Vector2 originBody = { dstBody.width / 2.0f, dstBody.height / 2.0f };

    //    DrawTexturePro(bodyTexture, srcBody, dstBody, originBody, angleDeg, WHITE);

    //    // ---- RUEDAS DELANTERAS ----
    //    float bodyW = dstBody.width;
    //    float bodyH = dstBody.height;

    //    // Offset desde el centro (ajústalo si hace falta)
    //    float forwardOffset = bodyW * 0.00f;   // hacia la punta del coche
    //    float halfTrack = bodyH * 0.00f;   // separación lateral de las ruedas

    //    float cosA = cosf(angle);
    //    float sinA = sinf(angle);

    //    // Rueda delantera derecha (desde nuestro punto de vista superior)
    //    Vector2 wheelRight; // <-- Identificador no declarado
    //    wheelRight.x = x + cosA * forwardOffset - sinA * (-halfTrack);
    //    wheelRight.y = y + sinA * forwardOffset + cosA * (-halfTrack);

    //    // Rueda delantera izquierda
    //    Vector2 wheelLeft; // <-- Identificador no declarado
    //    wheelLeft.x = x + cosA * forwardOffset - sinA * (halfTrack);
    //    wheelLeft.y = y + sinA * forwardOffset + cosA * (halfTrack);
    //    // **********************************************

    //    Rectangle srcWheel = { 0, 0, (float)frontTexture.width, (float)frontTexture.height };
    //    Rectangle dstWheel = { 0, 0,
    //                           frontTexture.width * scale,
    //                           frontTexture.height * scale };
    //    Vector2 originWheel = { dstWheel.width / 2.0f, dstWheel.height / 2.0f };

    //    float wheelAngleDeg = angleDeg + steeringVisual; // ruedas giran más

    //    // Seleccionar la textura de la rueda según el giro (basado en la respuesta anterior)
    //    Texture2D currentWheelTexture;
    //    if (steeringInput < 0.0f)
    //    {
    //        currentWheelTexture = gFrontCarTextureLeft;
    //    }
    //    else if (steeringInput > 0.0f)
    //    {
    //        currentWheelTexture = gFrontCarTextureRight;
    //    }
    //    else
    //    {
    //        currentWheelTexture = gFrontCarTexture;
    //    }

    //    // derecha
    //    dstWheel.x = wheelRight.x;
    //    dstWheel.y = wheelRight.y;
    //    DrawTexturePro(currentWheelTexture, srcWheel, dstWheel, originWheel, wheelAngleDeg, WHITE);

    //    // izquierda
    //    dstWheel.x = wheelLeft.x;
    //    dstWheel.y = wheelLeft.y;
    //    DrawTexturePro(currentWheelTexture, srcWheel, dstWheel, originWheel, wheelAngleDeg, WHITE);
    //}
    void DrawCar()
    {
        int x, y;
        body->GetPhysicPosition(x, y);
        float angle = body->GetRotation();
        float angleDeg = angle * RAD2DEG;

        float scale = 0.05f;   // Tamaño del coche

        // ===== CARROCERÍA =====
        Rectangle srcBody = { 0, 0, (float)bodyTexture.width, (float)bodyTexture.height };
        Rectangle dstBody = {
            (float)x, (float)y,
            bodyTexture.width * scale,
            bodyTexture.height * scale
        };
        Vector2 originBody = { dstBody.width / 2.0f, dstBody.height / 2.0f };

        DrawTexturePro(bodyTexture, srcBody, dstBody, originBody, angleDeg, WHITE);

        // ===== ELEGIR TEXTURA DEL MORRO =====
        Texture2D* frontTex = &gFrontCarTexture; // recto por defecto

        if (steeringInput < -0.1f)       // A
            frontTex = &gFrontCarTextureLeft;
        else if (steeringInput > 0.1f)   // D
            frontTex = &gFrontCarTextureRight;

        // ===== POSICIÓN DEL MORRO SOBRE LA CARROCERÍA =====
        float bodyW = dstBody.width;

        // cuánto sobresale el morro hacia delante (ajusta si hace falta)
        float forwardOffset = bodyW * 0.00f;

        float cosA = cosf(angle);
        float sinA = sinf(angle);

        Vector2 frontPos;
        frontPos.x = x + cosA * forwardOffset;
        frontPos.y = y + sinA * forwardOffset;

        // ===== DIBUJAR EL MORRO CON LA MISMA ROTACIÓN =====
        Rectangle srcFront = { 0, 0, (float)frontTex->width, (float)frontTex->height };
        Rectangle dstFront = {
            frontPos.x, frontPos.y,
            frontTex->width * scale,
            frontTex->height * scale
        };
        Vector2 originFront = { dstFront.width / 2.0f, dstFront.height / 2.0f };

        // OJO: aquí usamos SOLO angleDeg, nada de steeringVisual
        DrawTexturePro(*frontTex, srcFront, dstFront, originFront, angleDeg, WHITE);
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
    const float maxSpeed = 0.5f;
    const float moveFactor = 4.0f;   // escala para velocidad en Box2D
    const float baseTurnSpeedDeg = 2.0f;   // grados de giro base
    const float maxSteerVisualDeg = 15.0f;  // giro máximo de ruedas
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

    mapaMontmelo = LoadTexture("Assets/mapa_montmelo.png");
    if (mapaMontmelo.id == 0)
        LOG("No se ha podido cargar Assets/mapa_montmelo.png");

    // Texturas del coche
    carTexture = LoadTexture("Assets/f1_body_car.png");
    gFrontCarTexture = LoadTexture("Assets/f1_front_car.png");
    gFrontCarTextureLeft = LoadTexture("Assets/f1_front_car_Left.png");
    gFrontCarTextureRight = LoadTexture("Assets/f1_front_car_Right.png");



    if (gFrontCarTextureLeft.id == 0)
        LOG("No se ha podido cargar Assets/f1_front_car_Right.png");
    if (gFrontCarTextureRight.id == 0)
        LOG("Assets/f1_front_car_Left.png");
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

    // X Y ANCHURA ALTURA
    checkP1 = App->physics->CreateRectangleSensor(300, 300, 80, 40); checkP1->listener = this;
    checkP2 = App->physics->CreateRectangleSensor(800, 300, 80, 40); checkP2->listener = this;
    checkP3 = App->physics->CreateRectangleSensor(800, 600, 80, 40); checkP3->listener = this;

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

// Update: draw background + entities
update_status ModuleGame::Update()
{
    constexpr float MAP_SCALE = 4.0f;

    // --- Posición del coche en mundo (pixeles) ---
    int carPx, carPy;
    car->body->GetPhysicPosition(carPx, carPy);

    // --- Cámara fija al coche (coche centrado siempre) ---
    float screenW = (float)SCREEN_WIDTH;
    float screenH = (float)SCREEN_HEIGHT;

    App->renderer->camera.x = screenW * 0.5f - (float)carPx * MAP_SCALE;
    App->renderer->camera.y = screenH * 0.5f - (float)carPy * MAP_SCALE;

    //// --- Dibuja el mapa con la cámara aplicada ---
    //Vector2 mapPos = { App->renderer->camera.x, App->renderer->camera.y };
    //DrawTextureEx(mapaMontmelo, mapPos, 0.0f, MAP_SCALE, WHITE);
     //// ----- Circuito sencillo (puedes cambiarlo luego) -----

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

    for (PhysicEntity* entity : entities)
        entity->Update();

    DrawText(TextFormat("VUELTAS: %d", lapCount), 20, 40, 20, BLACK);
    DrawText(TextFormat("CHECKPOINT: %d/3", nextCheckpoint - 1), 20, 70, 20, BLACK);

    return UPDATE_CONTINUE;
}
  
void ModuleGame::OnCollision(PhysBody* bodyA, PhysBody* bodyB)
{
    // Queremos detectar cuando el coche toca un checkpoint:
    PhysBody* other = nullptr;

    // Si A es el coche
    if (bodyA == car->body) other = bodyB;
    // Si B es el coche
    else if (bodyB == car->body) other = bodyA;
    else return;

    // Checkpoints en orden
    if (nextCheckpoint == 1 && other == checkP1)
    {
        nextCheckpoint = 2;
    }
    else if (nextCheckpoint == 2 && other == checkP2)
    {
        nextCheckpoint = 3;
    }
    else if (nextCheckpoint == 3 && other == checkP3)
    {
        lapCount++;
        nextCheckpoint = 1; // vuelta completa
        App->audio->PlayFx(bonus_fx); 
    }
}

