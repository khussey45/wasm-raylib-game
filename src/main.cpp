
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

// --- gems (flight phase): fly into one to collect it for bonus points ---
struct Gem {
    Vector2 pos;
    float speed;      // px/s, moving left
    int type;         // index into gemTypes
};

struct GemType {
    Color color;
    int points;
    int weight;       // relative spawn chance
};

constexpr GemType gemTypes[] = {
    {GREEN,                     100, 60},  // common
    {Color{34, 230, 255, 255},  250, 30},  // uncommon (neon cyan)
    {Color{255, 43, 214, 255},  500, 10},  // rare (neon magenta)
};
constexpr int gemTypeCount = 3;
constexpr int gemWeightTotal = 100;       // sum of the weights above

constexpr float gemRadius = 9.0f;
constexpr float gemSpawnInterval = 2.5f;  // seconds between gems
constexpr float gemPickupRadius = gemRadius + 6.0f;  // forgiving pickup

// pickup pop: an expanding, fading diamond outline where a gem was collected
struct GemPop {
    Vector2 pos;
    int type;         // index into gemTypes, for the color
    float life;       // seconds remaining
};

constexpr float gemPopDuration = 0.25f;
constexpr float gemPopScale = 2.6f;  // outline grows to this multiple

// magnetism: gems near the ship drift toward it, so a close pass still collects
constexpr float gemMagnetRadius = 110.0f;    // pull starts inside this distance
constexpr float gemMagnetMaxPull = 900.0f;   // px/s at point-blank range

// ground gems float at jump height; a full jump peaks ~133 px above the
// ground (jumpImpulse^2 / (2 * gravity)), so keep them inside that arc
constexpr float gemGroundMinRise = 35.0f;
constexpr float gemGroundMaxRise = 120.0f;

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

// floating "+N" text where points were earned (enemy kill, gem pickup)
struct ScorePopup {
    Vector2 pos;
    float life;       // seconds remaining
    int value;        // points shown
    Color color;
};

constexpr float popupDuration = 0.9f;
constexpr float popupRiseSpeed = 40.0f;  // px/s upward drift

constexpr int deathParticleCount = 40;
constexpr float shakeDuration = 0.45f;
constexpr float shakeMagnitude = 8.0f;
constexpr float flashDuration = 0.25f;

// --- phase transition feedback: a colored flash + shockwave ring at the
// instant flight/ground switch, so the moment reads as an event, not a
// sudden rule change. Color matches the incoming phase (see DrawScene's
// phase warning banner: orange = ground incoming, sky blue = flight incoming).
constexpr float transitionFlashDuration = 0.35f;
constexpr float transitionShakeDuration = 0.3f;
constexpr float transitionRingDuration = 0.5f;
constexpr float transitionRingMaxRadius = 240.0f;

// --- touch controls (phones/tablets) ---
// Keyboard stays the primary input; on touch devices an on-screen joystick
// (left half) and a fire/jump button (right half) are shown as well.
constexpr Vector2 joystickBase{100.0f, screenHeight - 95.0f};
constexpr float joystickRadius = 55.0f;
constexpr float joystickDeadzone = 0.15f;   // fraction of radius ignored as noise
constexpr float touchSplitX = screenWidth * 0.45f;  // left of this = joystick
constexpr Vector2 actionButtonCenter{screenWidth - 75.0f, screenHeight - 80.0f};
constexpr float actionButtonRadius = 45.0f;
constexpr Rectangle pauseButtonRect{screenWidth / 2.0f - 30.0f, 0.0f, 60.0f, 44.0f};

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
static std::vector<Gem> gems;
static std::vector<GemPop> gemPops;
static std::vector<Particle> particles;
static std::vector<ScorePopup> popups;
static float survivalTime = 0.0f;   // drives the difficulty ramp
static float score = 0.0f;          // survival time + kill bonuses
static float bestScore = 0.0f;      // best ever (persisted)
static float spawnTimer = 0.0f;
static float enemySpawnTimer = 0.0f;
static float gemSpawnTimer = 0.0f;
static float fireTimer = 0.0f;
static float shakeTime = 0.0f;
static float flashTime = 0.0f;
static Color flashColor = RED;  // hit/death flash is red; transitions override this
static float transitionRingTime = 0.0f;
static Vector2 transitionRingPos{};
static Color transitionRingColor = RAYWHITE;

// touch input, refreshed once per frame in UpdateTouchInput()
static bool touchControlsVisible = false;  // true on touch devices
static Vector2 touchDir{0, 0};             // joystick deflection, length <= 1
static bool touchActionHeld = false;       // finger on the right half (fire)
static bool touchAnyHeld = false;          // any finger down (jump on the ground)
static bool touchJustTapped = false;       // a new touch began this frame
static bool touchPauseTapped = false;      // that new touch hit the pause button
static int prevTouchCount = 0;

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
// Touch-capable browser (iOS/iPadOS/Android)? Decides whether the on-screen
// controls are drawn at all.
EM_JS(int, DetectTouchDevice, (), {
    return ('ontouchstart' in window || navigator.maxTouchPoints > 0) ? 1 : 0;
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
    gems.clear();
    gemPops.clear();
    popups.clear();
    survivalTime = 0.0f;
    score = 0.0f;
    spawnTimer = 0.0f;
    enemySpawnTimer = 0.0f;
    gemSpawnTimer = 0.0f;
    fireTimer = 0.0f;
}

static void SwitchPhase() {
    // Clear the old phase's obstacles and give the player a moment before
    // the new phase starts spawning.
    obstacles.clear();
    spikes.clear();
    ramps.clear();
    enemies.clear();
    gems.clear();
    spawnTimer = -phaseSpawnGrace;
    enemySpawnTimer = 0.0f;
    gemSpawnTimer = 0.0f;
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

    // colored flash + shockwave ring centered on the ship, marking the switch
    const Color transitionColor = (phase == Phase::Ground) ? ORANGE : SKYBLUE;
    flashTime = transitionFlashDuration;
    flashColor = transitionColor;
    shakeTime = std::max(shakeTime, transitionShakeDuration);
    transitionRingTime = transitionRingDuration;
    transitionRingPos = shipPos;
    transitionRingColor = transitionColor;
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

static void SpawnGem() {
    // weighted pick: rarer colors are worth more
    int roll = GetRandomValue(0, gemWeightTotal - 1);
    int type = 0;
    for (int i = 0; i < gemTypeCount; i++) {
        if (roll < gemTypes[i].weight) { type = i; break; }
        roll -= gemTypes[i].weight;
    }
    float y;
    float speed;
    if (phase == Phase::Flying) {
        y = static_cast<float>(GetRandomValue(
            static_cast<int>(gemRadius), static_cast<int>(screenHeight - gemRadius)));
        // slightly slower than asteroids so gems feel catchable
        speed = (baseObstacleSpeed + speedPerSecond * survivalTime) * 0.8f;
    } else {
        // hover inside the jump arc and scroll with the terrain
        y = groundY - static_cast<float>(GetRandomValue(
            static_cast<int>(gemGroundMinRise), static_cast<int>(gemGroundMaxRise)));
        speed = baseSpikeSpeed + speedPerSecond * survivalTime;
    }
    gems.push_back({{screenWidth + gemRadius, y}, speed, type});
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
    flashColor = RED;
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
    flashColor = RED;
    SpawnBurst(shipPos, 12, SKYBLUE);
}

// Poll raylib's touch points once per frame and reduce them to simple
// intents (joystick direction, action held, taps) the game logic reads.
static void UpdateTouchInput() {
    const int count = GetTouchPointCount();
    if (count > 0) touchControlsVisible = true;  // fallback if detection missed

    touchDir = {0, 0};
    touchActionHeld = false;
    touchAnyHeld = false;
    touchJustTapped = (count > 0 && prevTouchCount == 0);
    touchPauseTapped = false;
    prevTouchCount = count;

    for (int i = 0; i < count; i++) {
        const Vector2 p = GetTouchPosition(i);
        if (touchJustTapped && CheckCollisionPointRec(p, pauseButtonRect)) {
            touchPauseTapped = true;
            continue;  // the pause tap shouldn't also steer or jump
        }
        touchAnyHeld = true;
        if (p.x < touchSplitX) {
            // joystick: deflection from the base, clamped to the rim
            Vector2 d{(p.x - joystickBase.x) / joystickRadius,
                      (p.y - joystickBase.y) / joystickRadius};
            const float len = sqrtf(d.x * d.x + d.y * d.y);
            if (len > 1.0f) { d.x /= len; d.y /= len; }
            if (len > joystickDeadzone) touchDir = d;
        } else {
            touchActionHeld = true;
        }
    }
}

static void UpdateCombat(float dt) {
    // --- firing (flight only): hold space, limited by cooldown ---
    fireTimer = std::max(0.0f, fireTimer - dt);
    if ((IsKeyDown(KEY_SPACE) || touchActionHeld) && fireTimer <= 0.0f) {
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
            popups.push_back({e.pos, popupDuration,
                              static_cast<int>(killBonus), GOLD});
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
    // touch joystick adds its (already length-clamped) deflection; keep the
    // combined thrust from exceeding full deflection
    dir.x += touchDir.x;
    dir.y += touchDir.y;
    const float dirLen = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (dirLen > 1.0f) { dir.x /= dirLen; dir.y /= dirLen; }
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

// Runs in both phases: spawn on a fixed interval, drift left, collect on contact.
static void UpdateGems(float dt) {
    gemSpawnTimer += dt;
    if (gemSpawnTimer >= gemSpawnInterval) {
        gemSpawnTimer = 0.0f;
        SpawnGem();
    }
    for (auto& g : gems) {
        g.pos.x -= g.speed * dt;
        // magnet: pull ramps from zero at the rim to full at the ship
        const float dx = shipPos.x - g.pos.x;
        const float dy = shipPos.y - g.pos.y;
        const float dist = sqrtf(dx * dx + dy * dy);
        if (dist < gemMagnetRadius && dist > 1.0f) {
            // sqrt falloff: strong pull through most of the radius, only
            // tapering right at the edge, so a fast pass-by still gets caught
            const float pull = gemMagnetMaxPull * sqrtf(1.0f - dist / gemMagnetRadius);
            g.pos.x += dx / dist * pull * dt;
            g.pos.y += dy / dist * pull * dt;
        }
    }
    std::erase_if(gems, [](const Gem& g) { return g.pos.x < -gemRadius; });
    std::erase_if(gems, [](const Gem& g) {
        if (!CheckCollisionCircles(shipPos, shipRadius, g.pos, gemPickupRadius)) {
            return false;
        }
        const GemType& t = gemTypes[g.type];
        score += static_cast<float>(t.points);
        popups.push_back({g.pos, popupDuration, t.points, t.color});
        gemPops.push_back({g.pos, g.type, gemPopDuration});
        SpawnBurst(g.pos, 8, t.color);
        return true;
    });
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
    if (grounded && (IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)
                     || touchAnyHeld)) {
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
    UpdateGems(dt);
}

// Effects keep animating even on the game-over screen.
static void UpdateEffects(float dt) {
    shakeTime = std::max(0.0f, shakeTime - dt);
    flashTime = std::max(0.0f, flashTime - dt);
    transitionRingTime = std::max(0.0f, transitionRingTime - dt);
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
    for (auto& gp : gemPops) gp.life -= dt;
    std::erase_if(gemPops, [](const GemPop& gp) { return gp.life <= 0; });
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

// Semi-transparent overlay showing where to put fingers. Only drawn on
// touch devices; keyboard players never see it.
static void DrawTouchControls() {
    if (!touchControlsVisible) return;

    // pause button: two bars, top-center
    const float bx = pauseButtonRect.x + pauseButtonRect.width / 2;
    DrawRectangle(static_cast<int>(bx) - 8, 12, 5, 18, Fade(RAYWHITE, 0.5f));
    DrawRectangle(static_cast<int>(bx) + 3, 12, 5, 18, Fade(RAYWHITE, 0.5f));

    if (state != GameState::Playing) return;

    if (phase == Phase::Flying) {
        // joystick: rim plus a knob that follows the deflection
        const bool active = (touchDir.x != 0.0f || touchDir.y != 0.0f);
        DrawCircleLinesV(joystickBase, joystickRadius, Fade(RAYWHITE, 0.35f));
        const Vector2 knob{joystickBase.x + touchDir.x * joystickRadius,
                           joystickBase.y + touchDir.y * joystickRadius};
        DrawCircleV(knob, 16.0f, Fade(SKYBLUE, active ? 0.55f : 0.25f));

        DrawCircleLinesV(actionButtonCenter, actionButtonRadius,
                         Fade(YELLOW, touchActionHeld ? 0.8f : 0.35f));
        DrawText("FIRE", static_cast<int>(actionButtonCenter.x) - MeasureText("FIRE", 16) / 2,
                 static_cast<int>(actionButtonCenter.y) - 8, 16,
                 Fade(YELLOW, touchActionHeld ? 0.9f : 0.4f));
    } else {
        DrawCircleLinesV(actionButtonCenter, actionButtonRadius,
                         Fade(SKYBLUE, touchAnyHeld ? 0.8f : 0.35f));
        DrawText("JUMP", static_cast<int>(actionButtonCenter.x) - MeasureText("JUMP", 16) / 2,
                 static_cast<int>(actionButtonCenter.y) - 8, 16,
                 Fade(SKYBLUE, touchAnyHeld ? 0.9f : 0.4f));
    }
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
    for (const auto& g : gems) {
        // diamond: a 4-sided polygon with a gentle pulse so gems catch the eye
        const float pulse = 1.0f + 0.15f * sinf(static_cast<float>(GetTime()) * 5.0f);
        DrawPoly(g.pos, 4, gemRadius * pulse, 0.0f, gemTypes[g.type].color);
        DrawPolyLines(g.pos, 4, gemRadius * pulse, 0.0f, RAYWHITE);
    }
    for (const auto& gp : gemPops) {
        const float t = 1.0f - gp.life / gemPopDuration;  // 0 → 1 over the pop
        const float size = gemRadius * (1.0f + (gemPopScale - 1.0f) * t);
        DrawPolyLines(gp.pos, 4, size, 0.0f, Fade(gemTypes[gp.type].color, 1.0f - t));
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
        const char* label = TextFormat("+%d", pop.value);
        DrawText(label, static_cast<int>(pop.pos.x) - MeasureText(label, 22) / 2,
                 static_cast<int>(pop.pos.y), 22, Fade(pop.color, pop.life / popupDuration));
    }
    // phase transition shockwave: an expanding ring from the ship's position
    // at the moment of the switch
    if (transitionRingTime > 0.0f) {
        const float t = 1.0f - transitionRingTime / transitionRingDuration;  // 0 → 1
        DrawCircleLinesV(transitionRingPos, transitionRingMaxRadius * t,
                         Fade(transitionRingColor, 1.0f - t));
        DrawCircleLinesV(transitionRingPos, transitionRingMaxRadius * t * 0.7f,
                         Fade(transitionRingColor, (1.0f - t) * 0.6f));
    }
    EndMode2D();

    // hit/death/transition flash: brief colored overlay
    if (flashTime > 0.0f) {
        DrawRectangle(0, 0, screenWidth, screenHeight,
                      Fade(flashColor, 0.35f * std::min(1.0f, flashTime / flashDuration)));
    }

    DrawText(TextFormat("SCORE %5.1f", score), 10, 10, 24, RAYWHITE);
    if (bestScore > 0.0f) {
        DrawText(TextFormat("BEST %5.1f", bestScore), 10, 38, 18, GRAY);
    }

    DrawTouchControls();

    // health: one ship glyph per HP, top-right
    for (int i = 0; i < health; i++) {
        const float x = screenWidth - 28.0f - i * 26.0f;
        DrawTriangle({x + 16, 22}, {x, 12}, {x, 32}, SKYBLUE);
    }

    // phase switch warning: pulsing border glow + banner during the last
    // seconds before a switch — hard to miss, but nothing over the play area
    if (state == GameState::Playing && phaseTimer <= phaseWarningTime) {
        // pulse speeds up as the switch gets closer
        const float urgency = 1.0f - phaseTimer / phaseWarningTime;  // 0 → 1
        const float pulse = 0.5f + 0.5f * sinf(static_cast<float>(GetTime())
                                               * (8.0f + 8.0f * urgency));
        const Color warnColor = (phase == Phase::Flying) ? ORANGE : SKYBLUE;
        DrawRectangleLinesEx({0, 0, screenWidth, screenHeight}, 6.0f,
                             Fade(warnColor, 0.15f + 0.45f * pulse));
        const char* warning = (phase == Phase::Flying)
            ? "! GRAVITY FAILING — BRACE FOR GROUND !"
            : "! THRUSTERS ONLINE — PREPARE FOR LIFT-OFF !";
        DrawRectangle(0, 60, screenWidth, 36, Fade(BLACK, 0.5f));
        DrawCenteredText(warning, 68, 22, Fade(warnColor, 0.6f + 0.4f * pulse));
    }

    if (state == GameState::Paused) {
        DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 160});
        DrawCenteredText("PAUSED", 170, 48, RAYWHITE);
        DrawCenteredText(touchControlsVisible ? "Press P or tap to resume"
                                              : "Press P to resume", 240, 20, GRAY);
    }

    if (state == GameState::GameOver) {
        DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 160});
        DrawCenteredText("GAME OVER", 150, 48, RED);
        DrawCenteredText(TextFormat("Final score: %.1f", score), 220, 24, RAYWHITE);
        if (score >= bestScore && bestScore > 0.0f) {
            DrawCenteredText("NEW BEST!", 250, 20, GOLD);
        }
        DrawCenteredText(touchControlsVisible ? "Tap to restart"
                                              : "Press ENTER or R to restart", 285, 20, GRAY);
    }
}

static void UpdateDrawFrame() {
    const float dt = GetFrameTime();
    UpdateTouchInput();

    switch (state) {
        case GameState::Playing:
            if (IsKeyPressed(KEY_P) || touchPauseTapped) state = GameState::Paused;
            else UpdatePlaying(dt);
            break;
        case GameState::Paused:
            if (IsKeyPressed(KEY_P) || touchJustTapped) state = GameState::Playing;
            break;
        case GameState::GameOver:
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_R) || touchJustTapped) ResetRun();
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
#if defined(PLATFORM_WEB)
    touchControlsVisible = (DetectTouchDevice() != 0);
#endif
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
