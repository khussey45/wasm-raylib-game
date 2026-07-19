
#include "raylib.h"
#include <string>
#include <vector>
#include <deque>

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

constexpr int screenWidth = 800;
constexpr int screenHeight = 450;
constexpr float radius = 20.0f;

static Vector2 pos{screenWidth / 2.0f, screenHeight / 2.0f};
static Vector2 vel{200.0f, 150.0f};

// --- "cout" log: lines of text output, most recent at the bottom ---
static std::deque<std::string> outputLog;
constexpr size_t maxLogLines = 8;

// --- "cin" input box: text being typed, submitted on Enter ---
static std::string inputBuffer;
static bool inputActive = true;
constexpr int maxInputChars = 60;

static void UpdateDrawFrame() {
    // --- update ball ---
    const float dt = GetFrameTime();
    pos.x += vel.x * dt;
    pos.y += vel.y * dt;
    if (pos.x - radius <= 0 || pos.x + radius >= screenWidth) vel.x *= -1;
    if (pos.y - radius <= 0 || pos.y + radius >= screenHeight) vel.y *= -1;

    // --- "cin" handling: capture typed characters ---
    if (inputActive) {
        int key = GetCharPressed();
        while (key > 0) {
            // only accept printable ASCII
            if (key >= 32 && key <= 125 && inputBuffer.size() < maxInputChars) {
                inputBuffer += static_cast<char>(key);
            }
            key = GetCharPressed(); // there can be multiple chars per frame
        }

        if (IsKeyPressed(KEY_BACKSPACE) && !inputBuffer.empty()) {
            inputBuffer.pop_back();
        }

        if (IsKeyPressed(KEY_ENTER) && !inputBuffer.empty()) {
            // this is your "cout" equivalent: push the line to the log
            outputLog.push_back("> " + inputBuffer);
            if (outputLog.size() > maxLogLines) outputLog.pop_front();
            inputBuffer.clear();
        }
    }

    // --- draw ---
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawCircleV(pos, radius, MAROON);
    DrawText("raylib + CMake template", 10, 10, 20, DARKGRAY);

    // draw output log ("cout")
    int logY = 60;
    for (const auto& line : outputLog) {
        DrawText(line.c_str(), 10, logY, 18, DARKGRAY);
        logY += 22;
    }

    // draw input box ("cin")
    Rectangle inputBox{10, screenHeight - 40.0f, screenWidth - 20.0f, 30.0f};
    DrawRectangleRec(inputBox, LIGHTGRAY);
    DrawRectangleLinesEx(inputBox, 2, inputActive ? MAROON : GRAY);
    DrawText(inputBuffer.c_str(), static_cast<int>(inputBox.x) + 5,
              static_cast<int>(inputBox.y) + 6, 18, BLACK);

    // blinking cursor
    if (inputActive && (static_cast<int>(GetTime() * 2) % 2 == 0)) {
        int textWidth = MeasureText(inputBuffer.c_str(), 18);
        DrawText("_", static_cast<int>(inputBox.x) + 8 + textWidth,
                  static_cast<int>(inputBox.y) + 6, 18, BLACK);
    }

    EndDrawing();
}

int main() {
    InitWindow(screenWidth, screenHeight, "raylib template");

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
