#include <cmath>
#include <cstring>
#include <ctime>
#include <string>

#include "chain.h"
#include "config.h"
#include "raylib.h"
#include "raymath.h"
#include "render.h"
#include "shooter.h"
#include "sound.h"
#include "track.h"

int RunSelfTests();

namespace {
void NewGame(Chain& chain, Shooter& shooter, const Track& track, unsigned seed) {
    ChainConfig c;
    c.trackLength   = track.Length();
    c.spacing       = cfg::SPACING;
    c.speed         = cfg::CHAIN_SPEED;
    c.introSpeed    = cfg::INTRO_SPEED;
    c.introCount    = cfg::INTRO_COUNT;
    c.slowStart     = cfg::SLOW_START;
    c.slowFactor    = cfg::SLOW_FACTOR;
    c.accel         = cfg::CHAIN_ACCEL;
    c.rollbackAccel = cfg::ROLLBACK_ACCEL;
    c.rollbackMax   = cfg::ROLLBACK_MAX;
    c.stopDecel     = cfg::STOP_DECEL;
    c.insertTime    = cfg::INSERT_TIME;
    c.popTime       = cfg::POP_TIME;
    c.drainSpeed    = cfg::DRAIN_SPEED;
    c.totalBalls    = cfg::TOTAL_BALLS;
    c.numColors     = cfg::NUM_COLORS;
    c.seed          = seed;
    chain.Reset(c);
    shooter.Reset(chain, seed * 7919u + 1u);
}
} // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--selftest") == 0) return RunSelfTests();

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Sphere Chain");
    SetTargetFPS(144);

    Sounds sounds;
    sounds.Load();

    Track track;
    track.Build(cfg::SPHERE_R, cfg::TRACK_ELEV_START, cfg::TRACK_ELEV_END, cfg::TRACK_TURNS, cfg::TRACK_SAMPLES);

    Renderer renderer;
    renderer.Init(track);

    Chain chain;
    Shooter shooter;
    unsigned seed = (unsigned)time(nullptr);
    NewGame(chain, shooter, track, seed);

    DisableCursor();
    float clock = 0.0f;

    while (!WindowShouldClose()) {
        float dt = std::fmin(GetFrameTime(), 1.0f / 30.0f);
        clock += dt;

        if (!IsCursorHidden() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) DisableCursor();
        if (IsKeyPressed(KEY_R)) NewGame(chain, shooter, track, ++seed);

        shooter.Look(GetMouseDelta());

        bool playing = !chain.Lost() && !chain.Won();
        if (playing) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && shooter.TryFire(chain)) sounds.Shoot();
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsKeyPressed(KEY_SPACE)) shooter.Swap();
            shooter.Update(dt, chain, track);
        }
        chain.Update(dt);

        for (const ChainEvent& e : chain.TakeEvents()) {
            Vector3 at = Vector3Scale(track.PosAt(e.s), 0.95f);
            switch (e.type) {
            case ChainEvent::Pop: {
                sounds.Pop(e.combo);
                std::string txt = "+" + std::to_string(e.points);
                if (e.combo > 0) txt += "  COMBO x" + std::to_string(e.combo);
                else if (e.streak >= 5) txt += "  STREAK x" + std::to_string(e.streak);
                renderer.AddPopup(at, txt, e.combo > 0 ? Color{255, 220, 80, 255} : RAYWHITE);
                break;
            }
            case ChainEvent::Inserted: sounds.Click(); break;
            case ChainEvent::Lost: sounds.Lose(); break;
            case ChainEvent::Won: sounds.Win(); break;
            case ChainEvent::ShotNoMatch: break;
            }
        }

        renderer.Update(dt);
        renderer.Draw(track, chain, shooter, clock);
    }

    renderer.Unload();
    sounds.Unload();
    CloseWindow();
    return 0;
}
