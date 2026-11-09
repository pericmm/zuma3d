#include "render.h"
#include "chain.h"
#include "config.h"
#include "shooter.h"
#include "track.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>

namespace {

const char* kVS = R"(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec3 fragPos;
out vec3 fragNormal;
void main() {
    fragPos = vec3(matModel * vec4(vertexPosition, 1.0));
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* kFS = R"(#version 330
in vec3 fragPos;
in vec3 fragNormal;
uniform vec4 colDiffuse;
uniform vec3 viewPos;
uniform vec3 lightPos;
out vec4 finalColor;
void main() {
    vec3 n = normalize(fragNormal);
    vec3 v = normalize(viewPos - fragPos);
    vec3 l = normalize(lightPos - fragPos);
    float diff = max(dot(n, l), 0.0);
    float head = max(dot(n, v), 0.0);
    float spec = pow(max(dot(n, normalize(l + v)), 0.0), 48.0);
    float rim  = pow(1.0 - head, 3.0);
    vec3 base = colDiffuse.rgb;
    vec3 col = base * (0.20 + 0.55 * diff + 0.35 * head) + vec3(spec * 0.6) + base * rim * 0.3;
    finalColor = vec4(col, colDiffuse.a);
}
)";

const Color kPalette[6] = {
    {230,  57,  70, 255},   // red
    {255, 200,  40, 255},   // yellow
    { 60, 200,  90, 255},   // green
    { 50, 130, 240, 255},   // blue
    {170,  80, 220, 255},   // purple
    {235, 235, 235, 255},   // white
};

void Basis(Vector3 n, Vector3& u, Vector3& v) {
    Vector3 ref = fabsf(n.y) < 0.9f ? Vector3{0, 1, 0} : Vector3{1, 0, 0};
    u = Vector3Normalize(Vector3CrossProduct(n, ref));
    v = Vector3CrossProduct(n, u);
}

void DrawRing(Vector3 c, Vector3 n, float r, Color col, int segs = 32) {
    Vector3 u, v;
    Basis(n, u, v);
    for (int i = 0; i < segs; ++i) {
        float a0 = 2 * PI * i / segs, a1 = 2 * PI * (i + 1) / segs;
        Vector3 p0 = Vector3Add(c, Vector3Add(Vector3Scale(u, cosf(a0) * r), Vector3Scale(v, sinf(a0) * r)));
        Vector3 p1 = Vector3Add(c, Vector3Add(Vector3Scale(u, cosf(a1) * r), Vector3Scale(v, sinf(a1) * r)));
        DrawLine3D(p0, p1, col);
    }
}

void DrawDisk(Vector3 c, Vector3 n, float r, Color col, int segs = 32) {
    Vector3 u, v;
    Basis(n, u, v);
    rlDisableBackfaceCulling();
    for (int i = 0; i < segs; ++i) {
        float a0 = 2 * PI * i / segs, a1 = 2 * PI * (i + 1) / segs;
        Vector3 p0 = Vector3Add(c, Vector3Add(Vector3Scale(u, cosf(a0) * r), Vector3Scale(v, sinf(a0) * r)));
        Vector3 p1 = Vector3Add(c, Vector3Add(Vector3Scale(u, cosf(a1) * r), Vector3Scale(v, sinf(a1) * r)));
        DrawTriangle3D(c, p0, p1, col);
    }
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}

Color Fade2(Color c, float a) { c.a = (unsigned char)(std::clamp(a, 0.0f, 1.0f) * 255); return c; }

} // namespace

Color BallColor(int c) { return kPalette[((c % 6) + 6) % 6]; }

void Renderer::Init(const Track& track) {
    shader_ = LoadShaderFromMemory(kVS, kFS);
    shader_.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader_, "viewPos");
    locLight_ = GetShaderLocation(shader_, "lightPos");
    sphere_ = LoadModelFromMesh(GenMeshSphere(1.0f, 20, 20));
    sphere_.materials[0].shader = shader_;

    // Track rails (just outside the ball silhouettes) and cross ties
    const float L = track.Length();
    const float R = track.Radius();
    for (float s = 0.0f; s <= L; s += 0.6f) {
        Vector3 p = track.PosAt(s);
        Vector3 n = Vector3Normalize(p);
        Vector3 side = Vector3Normalize(Vector3CrossProduct(n, track.TangentAt(s)));
        Vector3 base = Vector3Scale(n, R + 0.2f);
        railA_.push_back(Vector3Add(base, Vector3Scale(side, cfg::BALL_R * 1.2f)));
        railB_.push_back(Vector3Subtract(base, Vector3Scale(side, cfg::BALL_R * 1.2f)));
    }
    for (size_t i = 0; i < railA_.size(); i += 3) {
        ties_.push_back(Vector3Scale(railA_[i], 1.02f));
        ties_.push_back(Vector3Scale(railB_[i], 1.02f));
    }

    // Backdrop: latitude/longitude grid far behind the track
    const float G = R * 3.0f;
    auto sph = [&](float lat, float lon) {
        return Vector3{G * cosf(lat) * sinf(lon), G * sinf(lat), G * cosf(lat) * cosf(lon)};
    };
    for (int latD = -75; latD <= 75; latD += 15)
        for (int k = 0; k < 72; ++k) {
            float lat = latD * DEG2RAD;
            grid_.push_back(sph(lat, k * 5 * DEG2RAD));
            grid_.push_back(sph(lat, (k + 1) * 5 * DEG2RAD));
        }
    for (int lonD = 0; lonD < 360; lonD += 15)
        for (int k = -18; k < 18; ++k) {
            float lon = lonD * DEG2RAD;
            grid_.push_back(sph(k * 5 * DEG2RAD, lon));
            grid_.push_back(sph((k + 1) * 5 * DEG2RAD, lon));
        }
}

void Renderer::Unload() {
    UnloadModel(sphere_);   // also unloads the material's shader
}

void Renderer::Update(float dt) {
    for (auto& p : popups_) p.t += dt;
    popups_.erase(std::remove_if(popups_.begin(), popups_.end(), [](const Popup& p) { return p.t > 1.4f; }),
                  popups_.end());
}

void Renderer::AddPopup(Vector3 worldPos, const std::string& text, Color col) {
    popups_.push_back({worldPos, text, col, 0.0f});
}

void Renderer::DrawBall(Vector3 pos, float radius, Color col) {
    DrawModel(sphere_, pos, radius, col);
}

void Renderer::DrawWorld(const Track& track, const Chain& chain, const Shooter& shooter, float time) {
    const float R = track.Radius();

    // Backdrop grid, equator brighter
    for (size_t i = 0; i + 1 < grid_.size(); i += 2) {
        bool equator = fabsf(grid_[i].y) < 0.01f && fabsf(grid_[i + 1].y) < 0.01f;
        DrawLine3D(grid_[i], grid_[i + 1], equator ? Color{80, 100, 160, 255} : Color{32, 40, 70, 255});
    }

    // Rails and ties
    const Color rail{120, 110, 90, 255};
    for (size_t i = 0; i + 1 < railA_.size(); ++i) {
        DrawLine3D(railA_[i], railA_[i + 1], rail);
        DrawLine3D(railB_[i], railB_[i + 1], rail);
    }
    for (size_t i = 0; i + 1 < ties_.size(); i += 2) DrawLine3D(ties_[i], ties_[i + 1], Color{70, 62, 50, 255});

    // Entrance and pit
    Vector3 startN = Vector3Normalize(track.PosAt(0));
    DrawRing(Vector3Scale(startN, R + 0.1f), startN, 1.1f, Color{60, 200, 220, 255});
    Vector3 endN = Vector3Normalize(track.PosAt(track.Length()));
    DrawDisk(Vector3Scale(endN, R + 0.7f), endN, 1.3f, Color{20, 4, 8, 255});
    float pulse = 0.5f + 0.5f * sinf(time * 4.0f);
    DrawRing(Vector3Scale(endN, R + 0.6f), endN, 1.35f + 0.15f * pulse,
             Color{(unsigned char)(160 + 90 * pulse), 30, 40, 255});

    // Chain
    for (const Ball& b : chain.Balls()) {
        Vector3 p = track.PosAt(b.s);
        Color c = BallColor(b.color);
        float r = cfg::BALL_R;
        if (b.state == Ball::Inserting) r *= 0.6f + 0.4f * b.w;
        if (b.state == Ball::Popping) {
            r *= 1.0f + 0.3f * b.anim - 1.2f * b.anim * b.anim;
            c = ColorLerp(c, WHITE, b.anim);
            if (r <= 0.02f) continue;
        }
        DrawBall(p, r, c);
    }

    // Projectiles
    for (const Projectile& p : shooter.Shots()) DrawBall(p.pos, cfg::BALL_R, BallColor(p.color));

    // Held balls in view space (lower right)
    Vector3 f = shooter.Forward(), rt = shooter.Right(), up = shooter.Up();
    auto view = [&](float x, float y, float z) {
        return Vector3Add(Vector3Add(Vector3Scale(rt, x), Vector3Scale(up, y)), Vector3Scale(f, z));
    };
    DrawBall(view(0.42f, -0.30f, 1.0f), 0.11f, BallColor(shooter.Current()));
    DrawBall(view(0.58f, -0.36f, 1.0f), 0.06f, BallColor(shooter.Next()));
}

void Renderer::DrawHeadIndicator(const Track& track, const Chain& chain, const Shooter& shooter,
                                 const Camera3D& cam) {
    if (chain.Balls().empty()) return;
    const float W = (float)GetScreenWidth(), H = (float)GetScreenHeight();
    Vector3 P = track.PosAt(chain.Balls().front().s);
    float danger = chain.HeadProgress();
    Color col = danger > 0.85f ? Color{255, 60, 60, 255} : Color{255, 170, 40, 255};

    float x = Vector3DotProduct(P, shooter.Right());
    float y = Vector3DotProduct(P, shooter.Up());
    float z = Vector3DotProduct(P, shooter.Forward());
    const float margin = 40.0f;
    if (z > 0.0f) {
        Vector2 sp = GetWorldToScreen(P, cam);
        if (sp.x > margin && sp.x < W - margin && sp.y > margin && sp.y < H - margin) {
            DrawCircleLinesV(sp, 22.0f, col);
            DrawText("HEAD", (int)sp.x - 16, (int)sp.y - 40, 14, col);
            return;
        }
    }
    Vector2 d{x, -y};
    float len = sqrtf(d.x * d.x + d.y * d.y);
    if (len < 1e-3f) d = {1, 0}; else d = {d.x / len, d.y / len};
    Vector2 c{W * 0.5f, H * 0.5f};
    float tx = fabsf(d.x) > 1e-4f ? (W * 0.5f - margin) / fabsf(d.x) : 1e9f;
    float ty = fabsf(d.y) > 1e-4f ? (H * 0.5f - margin) / fabsf(d.y) : 1e9f;
    float t = std::min(tx, ty);
    Vector2 tip{c.x + d.x * t, c.y + d.y * t};
    Vector2 n{-d.y, d.x};
    Vector2 b0{tip.x - d.x * 28 + n.x * 14, tip.y - d.y * 28 + n.y * 14};
    Vector2 b1{tip.x - d.x * 28 - n.x * 14, tip.y - d.y * 28 - n.y * 14};
    DrawTriangle(tip, b1, b0, col);
    DrawTriangle(tip, b0, b1, col);   // either winding
}

void Renderer::DrawHud(const Track& track, const Chain& chain, const Shooter& shooter, const Camera3D& cam) {
    const int W = GetScreenWidth(), H = GetScreenHeight();

    // Crosshair tinted with the loaded ball
    Color cc = BallColor(shooter.Current());
    DrawCircleLines(W / 2, H / 2, 10, cc);
    DrawCircle(W / 2, H / 2, 2, cc);

    // Popups
    for (const auto& p : popups_) {
        Vector3 rel = p.pos;
        if (Vector3DotProduct(rel, shooter.Forward()) <= 0.0f) continue;
        Vector2 sp = GetWorldToScreen(p.pos, cam);
        int fs = 22;
        int tw = MeasureText(p.text.c_str(), fs);
        DrawText(p.text.c_str(), (int)sp.x - tw / 2, (int)(sp.y - 40 - p.t * 40), fs, Fade2(p.col, 1.4f - p.t));
    }

    // Score and info
    DrawText(TextFormat("SCORE %d", chain.Score()), 20, 16, 28, RAYWHITE);
    DrawText(TextFormat("Balls to come: %d", chain.Remaining()), 20, 50, 18, LIGHTGRAY);
    if (chain.Streak() >= 2) DrawText(TextFormat("Streak x%d", chain.Streak()), 20, 74, 18, GOLD);

    // Danger bar: how far the head is along the track
    float prog = chain.HeadProgress();
    int bw = 360, bh = 12, bx = W / 2 - bw / 2, by = 20;
    DrawRectangle(bx, by, bw, bh, Color{40, 40, 50, 200});
    Color fill = ColorLerp(Color{70, 200, 90, 255}, Color{240, 50, 50, 255}, prog);
    DrawRectangle(bx, by, (int)(bw * prog), bh, fill);
    DrawRectangleLines(bx, by, bw, bh, GRAY);
    DrawText("PIT", bx + bw + 8, by - 2, 16, Color{240, 80, 80, 255});

    DrawHeadIndicator(track, chain, shooter, cam);

    DrawText("LMB shoot   RMB/Space swap   R restart   Esc quit", 20, H - 28, 16, Color{150, 150, 170, 255});
    DrawFPS(W - 90, H - 28);

    auto banner = [&](const char* title, const char* sub, Color col) {
        DrawRectangle(0, H / 2 - 70, W, 140, Color{0, 0, 0, 170});
        int tw = MeasureText(title, 48);
        DrawText(title, W / 2 - tw / 2, H / 2 - 50, 48, col);
        int sw = MeasureText(sub, 20);
        DrawText(sub, W / 2 - sw / 2, H / 2 + 14, 20, RAYWHITE);
    };
    if (chain.Won())
        banner("CHAIN CLEARED!", TextFormat("Score %d  -  press R to play again", chain.Score()), GOLD);
    else if (chain.Lost())
        banner("THE CHAIN REACHED THE PIT", TextFormat("Score %d  -  press R to retry", chain.Score()),
               Color{255, 90, 90, 255});
}

void Renderer::Draw(const Track& track, const Chain& chain, const Shooter& shooter, float time) {
    Camera3D cam = shooter.MakeCamera();
    Vector3 light = {0.0f, track.Radius() * 0.7f, 0.0f};   // fixed overhead light inside the sphere
    SetShaderValue(shader_, locLight_, &light, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader_, shader_.locs[SHADER_LOC_VECTOR_VIEW], &cam.position, SHADER_UNIFORM_VEC3);

    BeginDrawing();
    ClearBackground(Color{8, 10, 22, 255});
    BeginMode3D(cam);
    DrawWorld(track, chain, shooter, time);
    EndMode3D();
    DrawHud(track, chain, shooter, cam);
    EndDrawing();
}
