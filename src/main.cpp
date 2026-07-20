
#include "raylib.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <fstream>

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

// --- screen ---
constexpr int screenWidth = 800;
constexpr int screenHeight = 450;

// --- ship handling (flight): acceleration + drag instead of raw velocity ---
constexpr float shipRadius = 12.0f;    // collision radius
constexpr float shipAccel = 1600.0f;   // px/s^2 while a key is held
constexpr float shipDrag = 4.0f;       // fraction of velocity shed per second

// --- health ---
constexpr int maxHealth = 3;
constexpr float invulnDuration = 1.5f;  // seconds of immunity after taking a hit

// --- phases: flight alternates with a ground-run section ---
enum class Phase { Flying, Ground };

constexpr float flightPhaseDuration = 20.0f;  // seconds of flying
constexpr float groundPhaseDuration = 36.0f;  // seconds of ground running
constexpr float phaseWarningTime = 2.0f;      // warning shown this long before a switch
constexpr float phaseSpawnGrace = 1.2f;       // no spawns right after a switch

// --- ground physics (Geometry-Dash-like) ---
constexpr float groundY = screenHeight - 50.0f;  // top of the ground strip
constexpr float gravity = 2400.0f;               // px/s^2
constexpr float jumpImpulse = 800.0f;            // px/s upward
constexpr float groundShipX = screenWidth / 4.0f;

// --- flying obstacles (asteroids) ---
struct Obstacle {
    Vector2 pos;
    float radius;
    float speed;      // px/s, moving left
};

constexpr float obstacleMinRadius = 10.0f;
constexpr float obstacleMaxRadius = 35.0f;

// --- ground obstacles (spikes) ---
struct Spike {
    float x;          // left edge
    float width;
    float height;
    float speed;      // px/s, moving left
};

// Ramps are safe terrain: the ship rides up the slope and launches off the top.
struct Ramp {
    float x;          // left edge (the low end)
    float width;
    float height;     // rise at the right edge
    float speed;      // px/s, moving left
};

constexpr int rampSpawnChance = 30;  // percent of ground spawns that are ramps

// --- combat (flight phase only) ---
struct Bullet {
    Vector2 pos;
};

struct Enemy {
    Vector2 pos;
    float baseY;      // center of the sine-wave path
    float age;        // seconds alive, drives the wave
    float speed;      // px/s, moving left
    int hp;
};

constexpr float bulletSpeed = 700.0f;
constexpr float bulletRadius = 4.0f;
constexpr float fireCooldown = 0.25f;
constexpr float enemyRadius = 16.0f;
constexpr int enemyMaxHp = 3;
constexpr float enemyBaseSpeed = 70.0f;
constexpr float enemyWaveAmplitude = 55.0f;
constexpr float enemySpawnInterval = 5.0f;   // seconds between enemies in flight
constexpr float killBonus = 500.0f;          // score awarded per enemy destroyed

// --- difficulty ramp ---
// Everything starts slow and gradually speeds up as survival time grows.
constexpr float baseObstacleSpeed = 90.0f;
constexpr float speedPerSecond = 4.0f;        // extra px/s per second survived
constexpr float baseSpawnInterval = 1.5f;     // seconds between spawns at start
constexpr float minSpawnInterval = 0.35f;
constexpr float spawnRampPerSecond = 0.010f;  // interval shrink per second survived

constexpr float baseSpikeSpeed = 260.0f;
constexpr float baseSpikeInterval = 1.5f;
constexpr float minSpikeInterval = 0.55f;

// --- death feedback ---
struct Particle {
    Vector2 pos;
    Vector2 vel;
    float life;       // seconds remaining
    float maxLife;
    Color color;
};

// floating "+500" text where an enemy died
struct ScorePopup {
    Vector2 pos;
    float life;       // seconds remaining
};

constexpr float popupDuration = 0.9f;
constexpr float popupRiseSpeed = 40.0f;  // px/s upward drift

constexpr int deathParticleCount = 40;
constexpr float shakeDuration = 0.45f;
constexpr float shakeMagnitude = 8.0f;
constexpr float flashDuration = 0.25f;

// --- game state ---
enum class GameState { Playing, Paused, GameOver };

static GameState state = GameState::Playing;
static Phase phase = Phase::Flying;
static float phaseTimer = flightPhaseDuration;  // counts down to the next switch
static Vector2 shipPos{};
static Vector2 shipVel{};
static bool grounded = false;
static int health = maxHealth;
static float invulnTime = 0.0f;
static std::vector<Obstacle> obstacles;
static std::vector<Spike> spikes;
static std::vector<Ramp> ramps;
static std::vector<Bullet> bullets;
static std::vector<Enemy> enemies;
static std::vector<Particle> particles;
static std::vector<ScorePopup> popups;
static float survivalTime = 0.0f;   // drives the difficulty ramp
static float score = 0.0f;          // survival time + kill bonuses
static float bestScore = 0.0f;      // best ever (persisted)
static float spawnTimer = 0.0f;
static float enemySpawnTimer = 0.0f;
static float fireTimer = 0.0f;
static float shakeTime = 0.0f;
static float flashTime = 0.0f;

// --- high score persistence ---
// Web: browser localStorage. Native: a small text file next to the working dir.
#if defined(PLATFORM_WEB)
EM_JS(float, LoadBestScore, (), {
    var v = localStorage.getItem('game-raylib-best');
    return v ? parseFloat(v) : 0.0;
});
EM_JS(void, SaveBestScore, (float value), {
    localStorage.setItem('game-raylib-best', value);
});
#else
static const char* bestScoreFile = "highscore.txt";

static float LoadBestScore() {
    std::ifstream in(bestScoreFile);
    float value = 0.0f;
    if (in) in >> value;
    return value;
}

static void SaveBestScore(float value) {
    std::ofstream out(bestScoreFile);
    if (out) out << value;
}
#endif

static void ResetRun() {
    state = GameState::Playing;
    phase = Phase::Flying;
    phaseTimer = flightPhaseDuration;
    shipPos = {screenWidth / 4.0f, screenHeight / 2.0f};
    shipVel = {0, 0};
    grounded = false;
    health = maxHealth;
    invulnTime = 0.0f;
    obstacles.clear();
    spikes.clear();
    ramps.clear();
    bullets.clear();
    enemies.clear();
    popups.clear();
    survivalTime = 0.0f;
    score = 0.0f;
    spawnTimer = 0.0f;
    enemySpawnTimer = 0.0f;
    fireTimer = 0.0f;
}

static void SwitchPhase() {
    // Clear the old phase's obstacles and give the player a moment before
    // the new phase starts spawning.
    obstacles.clear();
    spikes.clear();
    ramps.clear();
    enemies.clear();
    spawnTimer = -phaseSpawnGrace;
    enemySpawnTimer = 0.0f;
    if (phase == Phase::Flying) {
        phase = Phase::Ground;
        phaseTimer = groundPhaseDuration;
        // gravity takes over; the ship keeps its velocity and falls
    } else {
        phase = Phase::Flying;
        phaseTimer = flightPhaseDuration;
        grounded = false;
        shipVel.y = -200.0f;  // small lift-off kick
    }
}

static void SpawnObstacle() {
    const float radius = static_cast<float>(GetRandomValue(
        static_cast<int>(obstacleMinRadius), static_cast<int>(obstacleMaxRadius)));
    const float y = static_cast<float>(GetRandomValue(
        static_cast<int>(radius), static_cast<int>(screenHeight - radius)));
    const float speed = baseObstacleSpeed + speedPerSecond * survivalTime
                        + static_cast<float>(GetRandomValue(-20, 40));
    obstacles.push_back({{screenWidth + radius, y}, radius, speed});
}

static void SpawnSpike() {
    const float speed = baseSpikeSpeed + speedPerSecond * survivalTime;
    const float width = 26.0f;
    const float height = static_cast<float>(GetRandomValue(22, 40));
    float x = screenWidth + width;
    spikes.push_back({x, width, height, speed});
    // sometimes a double spike, spaced so a single jump can still clear it
    if (GetRandomValue(0, 99) < 30) {
        spikes.push_back({x + width + 6.0f, width,
                          static_cast<float>(GetRandomValue(22, 40)), speed});
    }
}

static void SpawnRamp() {
    const float speed = baseSpikeSpeed + speedPerSecond * survivalTime;
    const float width = static_cast<float>(GetRandomValue(90, 150));
    const float height = static_cast<float>(GetRandomValue(45, 85));
    ramps.push_back({static_cast<float>(screenWidth), width, height, speed});
}

static void SpawnEnemy() {
    const float margin = enemyRadius + enemyWaveAmplitude;
    const float baseY = static_cast<float>(GetRandomValue(
        static_cast<int>(margin), static_cast<int>(screenHeight - margin)));
    const float speed = enemyBaseSpeed + speedPerSecond * 0.5f * survivalTime;
    enemies.push_back({{screenWidth + enemyRadius, baseY}, baseY, 0.0f, speed, enemyMaxHp});
}

static void SpawnBurst(Vector2 at, int count, Color color) {
    for (int i = 0; i < count; i++) {
        const float angle = static_cast<float>(GetRandomValue(0, 359)) * DEG2RAD;
        const float speed = static_cast<float>(GetRandomValue(60, 360));
        const float life = static_cast<float>(GetRandomValue(30, 80)) / 100.0f;
        particles.push_back({at,
                             {speed * cosf(angle), speed * sinf(angle)},
                             life, life, color});
    }
}

static void OnDeath() {
    state = GameState::GameOver;
    shakeTime = shakeDuration;
    flashTime = flashDuration;
    SpawnBurst(shipPos, deathParticleCount, ORANGE);
    if (score > bestScore) {
        bestScore = score;
        SaveBestScore(bestScore);
    }
}

// A hit costs one health point and grants brief immunity; at zero the run ends.
static void TakeDamage() {
    if (invulnTime > 0.0f) return;
    health--;
    if (health <= 0) {
        OnDeath();
        return;
    }
    invulnTime = invulnDuration;
    shakeTime = shakeDuration * 0.6f;
    flashTime = flashDuration * 0.6f;
    SpawnBurst(shipPos, 12, SKYBLUE);
}

static void UpdateCombat(float dt) {
    // --- firing (flight only): hold space, limited by cooldown ---
    fireTimer = std::max(0.0f, fireTimer - dt);
    if (IsKeyDown(KEY_SPACE) && fireTimer <= 0.0f) {
        bullets.push_back({{shipPos.x + shipRadius + 6, shipPos.y}});
        fireTimer = fireCooldown;
    }

    // --- bullets fly right, despawn off-screen ---
    for (auto& b : bullets) b.pos.x += bulletSpeed * dt;
    std::erase_if(bullets, [](const Bullet& b) {
        return b.pos.x > screenWidth + bulletRadius;
    });

    // --- enemies drift left on a sine wave ---
    enemySpawnTimer += dt;
    if (enemySpawnTimer >= enemySpawnInterval) {
        enemySpawnTimer = 0.0f;
        SpawnEnemy();
    }
    for (auto& e : enemies) {
        e.age += dt;
        e.pos.x -= e.speed * dt;
        e.pos.y = e.baseY + enemyWaveAmplitude * sinf(2.0f * e.age);
    }
    std::erase_if(enemies, [](const Enemy& e) {
        return e.pos.x < -enemyRadius;
    });

    // --- bullets vs enemies ---
    for (auto& e : enemies) {
        for (auto it = bullets.begin(); it != bullets.end();) {
            if (CheckCollisionCircles(it->pos, bulletRadius, e.pos, enemyRadius)) {
                e.hp--;
                SpawnBurst(it->pos, 4, PURPLE);
                it = bullets.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (const auto& e : enemies) {
        if (e.hp <= 0) {
            SpawnBurst(e.pos, 20, PURPLE);
            score += killBonus;
            popups.push_back({e.pos, popupDuration});
        }
    }
    std::erase_if(enemies, [](const Enemy& e) { return e.hp <= 0; });

    // --- enemies ramming the ship ---
    for (auto it = enemies.begin(); it != enemies.end(); ++it) {
        if (CheckCollisionCircles(shipPos, shipRadius, it->pos, enemyRadius)) {
            SpawnBurst(it->pos, 20, PURPLE);
            enemies.erase(it);  // the enemy dies in the ram
            TakeDamage();
            break;
        }
    }
}

static void UpdateFlying(float dt) {
    // --- thrust in held direction, drag slows the rest ---
    Vector2 dir{0, 0};
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) dir.x += 1;
    if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A)) dir.x -= 1;
    if (IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S)) dir.y += 1;
    if (IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W)) dir.y -= 1;
    if (dir.x != 0 && dir.y != 0) {
        // normalize diagonals so they aren't faster
        constexpr float invSqrt2 = 0.70710678f;
        dir.x *= invSqrt2;
        dir.y *= invSqrt2;
    }
    shipVel.x += dir.x * shipAccel * dt;
    shipVel.y += dir.y * shipAccel * dt;
    const float dragFactor = 1.0f / (1.0f + shipDrag * dt);
    shipVel.x *= dragFactor;
    shipVel.y *= dragFactor;

    shipPos.x += shipVel.x * dt;
    shipPos.y += shipVel.y * dt;
    // clamp to screen; kill velocity into the wall so the ship doesn't stick
    if (shipPos.x < shipRadius)                { shipPos.x = shipRadius;                shipVel.x = 0; }
    if (shipPos.x > screenWidth - shipRadius)  { shipPos.x = screenWidth - shipRadius;  shipVel.x = 0; }
    if (shipPos.y < shipRadius)                { shipPos.y = shipRadius;                shipVel.y = 0; }
    if (shipPos.y > screenHeight - shipRadius) { shipPos.y = screenHeight - shipRadius; shipVel.y = 0; }

    // --- spawn asteroids on a shrinking interval ---
    const float spawnInterval = std::max(
        minSpawnInterval, baseSpawnInterval - spawnRampPerSecond * survivalTime);
    spawnTimer += dt;
    while (spawnTimer >= spawnInterval) {
        spawnTimer -= spawnInterval;
        SpawnObstacle();
    }

    // --- move asteroids, drop the ones past the left edge ---
    for (auto& obs : obstacles) obs.pos.x -= obs.speed * dt;
    std::erase_if(obstacles, [](const Obstacle& obs) {
        return obs.pos.x < -obs.radius;
    });

    // --- asteroid hit: costs health, destroys the asteroid ---
    for (auto it = obstacles.begin(); it != obstacles.end(); ++it) {
        if (CheckCollisionCircles(shipPos, shipRadius, it->pos, it->radius)) {
            SpawnBurst(it->pos, 10, GRAY);
            obstacles.erase(it);
            TakeDamage();
            break;
        }
    }

    UpdateCombat(dt);
}

static void UpdateGround(float dt) {
    // --- the ship slides to its running position; only jumping is controlled ---
    shipPos.x += (groundShipX - shipPos.x) * std::min(1.0f, 4.0f * dt);

    // --- spawn ground obstacles (spikes or ramps) on a shrinking interval ---
    const float spikeInterval = std::max(
        minSpikeInterval, baseSpikeInterval - spawnRampPerSecond * survivalTime);
    spawnTimer += dt;
    while (spawnTimer >= spikeInterval) {
        spawnTimer -= spikeInterval;
        if (GetRandomValue(0, 99) < rampSpawnChance) SpawnRamp();
        else SpawnSpike();
    }

    // --- move ground obstacles, drop the ones past the left edge ---
    for (auto& s : spikes) s.x -= s.speed * dt;
    std::erase_if(spikes, [](const Spike& s) { return s.x + s.width < 0; });
    for (auto& r : ramps) r.x -= r.speed * dt;
    std::erase_if(ramps, [](const Ramp& r) { return r.x + r.width < 0; });

    // --- the surface under the ship: flat ground, unless a ramp is passing below ---
    float supportY = groundY;
    float rideVel = 0.0f;  // upward speed the moving slope imparts while riding it
    for (const auto& r : ramps) {
        if (shipPos.x >= r.x && shipPos.x <= r.x + r.width) {
            const float surface = groundY - r.height * (shipPos.x - r.x) / r.width;
            if (surface < supportY) {
                supportY = surface;
                rideVel = -(r.height / r.width) * r.speed;
            }
        }
    }

    // gravity + jump
    shipVel.y += gravity * dt;
    if (grounded && (IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))) {
        shipVel.y = -jumpImpulse;
        grounded = false;
    }
    shipPos.y += shipVel.y * dt;
    if (shipPos.y + shipRadius >= supportY) {
        shipPos.y = supportY - shipRadius;
        // on a ramp the slope carries the ship upward, so leaving the top
        // launches it; on flat ground the ship just rests
        shipVel.y = rideVel;
        grounded = true;
    } else if (shipPos.y + shipRadius < supportY - 1.0f) {
        grounded = false;  // walked off a ramp edge: airborne, no double jump
    }

    // --- spike hit: costs health (hitbox slimmer than drawn, for fairness) ---
    for (const auto& s : spikes) {
        const Rectangle hitbox{s.x + s.width * 0.25f, groundY - s.height * 0.8f,
                               s.width * 0.5f, s.height * 0.8f};
        if (CheckCollisionCircleRec(shipPos, shipRadius * 0.85f, hitbox)) {
            TakeDamage();
            break;
        }
    }
}

static void UpdatePlaying(float dt) {
    survivalTime += dt;
    score += dt;
    invulnTime = std::max(0.0f, invulnTime - dt);

    phaseTimer -= dt;
    if (phaseTimer <= 0.0f) SwitchPhase();

    if (phase == Phase::Flying) UpdateFlying(dt);
    else UpdateGround(dt);
}

// Effects keep animating even on the game-over screen.
static void UpdateEffects(float dt) {
    shakeTime = std::max(0.0f, shakeTime - dt);
    flashTime = std::max(0.0f, flashTime - dt);
    for (auto& p : particles) {
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        p.life -= dt;
    }
    std::erase_if(particles, [](const Particle& p) { return p.life <= 0; });
    for (auto& pop : popups) {
        pop.pos.y -= popupRiseSpeed * dt;
        pop.life -= dt;
    }
    std::erase_if(popups, [](const ScorePopup& p) { return p.life <= 0; });
}

static void DrawShip() {
    // blink while invulnerable
    if (invulnTime > 0.0f && static_cast<int>(GetTime() * 10) % 2 == 0) return;
    // triangle pointing right, sized around the collision radius
    const Vector2 nose{shipPos.x + shipRadius + 4, shipPos.y};
    const Vector2 backTop{shipPos.x - shipRadius, shipPos.y - shipRadius};
    const Vector2 backBottom{shipPos.x - shipRadius, shipPos.y + shipRadius};
    DrawTriangle(nose, backTop, backBottom, SKYBLUE);
    DrawTriangleLines(nose, backTop, backBottom, RAYWHITE);
}

static void DrawCenteredText(const char* text, int y, int size, Color color) {
    DrawText(text, (screenWidth - MeasureText(text, size)) / 2, y, size, color);
}

static void DrawScene() {
    ClearBackground(Color{15, 15, 30, 255});

    // screen shake: jitter the whole world while the timer runs
    Camera2D camera{};
    camera.zoom = 1.0f;
    if (shakeTime > 0.0f) {
        const float strength = shakeMagnitude * (shakeTime / shakeDuration);
        camera.offset.x = static_cast<float>(GetRandomValue(-100, 100)) / 100.0f * strength;
        camera.offset.y = static_cast<float>(GetRandomValue(-100, 100)) / 100.0f * strength;
    }

    BeginMode2D(camera);

    if (phase == Phase::Ground) {
        DrawRectangle(0, static_cast<int>(groundY), screenWidth,
                      screenHeight - static_cast<int>(groundY), Color{40, 40, 60, 255});
        DrawLine(0, static_cast<int>(groundY), screenWidth, static_cast<int>(groundY), LIGHTGRAY);
        for (const auto& r : ramps) {
            DrawTriangle({r.x, groundY},
                         {r.x + r.width, groundY},
                         {r.x + r.width, groundY - r.height}, Color{20, 70, 90, 255});
            DrawLineV({r.x, groundY}, {r.x + r.width, groundY - r.height},
                      Color{34, 230, 255, 255});
        }
        for (const auto& s : spikes) {
            DrawTriangle({s.x, groundY},
                         {s.x + s.width, groundY},
                         {s.x + s.width / 2, groundY - s.height}, MAROON);
        }
    }

    for (const auto& obs : obstacles) {
        DrawCircleV(obs.pos, obs.radius, GRAY);
        DrawCircleLinesV(obs.pos, obs.radius, LIGHTGRAY);
    }
    for (const auto& b : bullets) {
        DrawCircleV(b.pos, bulletRadius, YELLOW);
    }
    for (const auto& e : enemies) {
        DrawCircleV(e.pos, enemyRadius, Color{120, 40, 140, 255});
        DrawCircleLinesV(e.pos, enemyRadius, PURPLE);
        // hp pips above the enemy
        for (int i = 0; i < e.hp; i++) {
            DrawRectangle(static_cast<int>(e.pos.x) - 9 + i * 7,
                          static_cast<int>(e.pos.y - enemyRadius) - 8, 5, 4, PURPLE);
        }
    }
    if (state != GameState::GameOver) DrawShip();
    for (const auto& p : particles) {
        const float t = p.life / p.maxLife;  // 1 → 0 over lifetime
        DrawCircleV(p.pos, 2.0f + 2.0f * t, Fade(p.color, t));
    }
    for (const auto& pop : popups) {
        const char* label = TextFormat("+%d", static_cast<int>(killBonus));
        DrawText(label, static_cast<int>(pop.pos.x) - MeasureText(label, 22) / 2,
                 static_cast<int>(pop.pos.y), 22, Fade(GOLD, pop.life / popupDuration));
    }
    EndMode2D();

    // hit flash: brief red overlay right after damage/death
    if (flashTime > 0.0f) {
        DrawRectangle(0, 0, screenWidth, screenHeight,
                      Fade(RED, 0.35f * (flashTime / flashDuration)));
    }

    DrawText(TextFormat("SCORE %5.1f", score), 10, 10, 24, RAYWHITE);
    if (bestScore > 0.0f) {
        DrawText(TextFormat("BEST %5.1f", bestScore), 10, 38, 18, GRAY);
    }

    // health: one ship glyph per HP, top-right
    for (int i = 0; i < health; i++) {
        const float x = screenWidth - 28.0f - i * 26.0f;
        DrawTriangle({x + 16, 22}, {x, 12}, {x, 32}, SKYBLUE);
    }

    // phase switch warning: blink during the last seconds before a switch
    if (state == GameState::Playing && phaseTimer <= phaseWarningTime) {
        if (static_cast<int>(GetTime() * 5) % 2 == 0) {
            const char* warning = (phase == Phase::Flying)
                ? "! GRAVITY FAILING — BRACE FOR GROUND !"
                : "! THRUSTERS ONLINE — PREPARE FOR LIFT-OFF !";
            DrawCenteredText(warning, 70, 20, YELLOW);
        }
    }

    if (state == GameState::Paused) {
        DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 160});
        DrawCenteredText("PAUSED", 170, 48, RAYWHITE);
        DrawCenteredText("Press P to resume", 240, 20, GRAY);
    }

    if (state == GameState::GameOver) {
        DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 160});
        DrawCenteredText("GAME OVER", 150, 48, RED);
        DrawCenteredText(TextFormat("Final score: %.1f", score), 220, 24, RAYWHITE);
        if (score >= bestScore && bestScore > 0.0f) {
            DrawCenteredText("NEW BEST!", 250, 20, GOLD);
        }
        DrawCenteredText("Press ENTER or R to restart", 285, 20, GRAY);
    }
}

static void UpdateDrawFrame() {
    const float dt = GetFrameTime();

    switch (state) {
        case GameState::Playing:
            if (IsKeyPressed(KEY_P)) state = GameState::Paused;
            else UpdatePlaying(dt);
            break;
        case GameState::Paused:
            if (IsKeyPressed(KEY_P)) state = GameState::Playing;
            break;
        case GameState::GameOver:
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_R)) ResetRun();
            break;
    }

    if (state != GameState::Paused) UpdateEffects(dt);

    BeginDrawing();
    DrawScene();
    EndDrawing();
}

int main() {
    InitWindow(screenWidth, screenHeight, "Fly Fall");
    bestScore = LoadBestScore();
    ResetRun();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        UpdateDrawFrame();
    }
#endif

    CloseWindow();
    return 0;
}
