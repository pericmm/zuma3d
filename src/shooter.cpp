#include "shooter.h"
#include "chain.h"
#include "config.h"
#include "track.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

namespace {
// Earliest t in [0,1] where segment p0->p1 comes within r of c, or -1.
float SweepSphere(Vector3 p0, Vector3 p1, Vector3 c, float r) {
    Vector3 d = Vector3Subtract(p1, p0);
    Vector3 m = Vector3Subtract(p0, c);
    float cc = Vector3DotProduct(m, m) - r * r;
    if (cc <= 0.0f) return 0.0f;
    float a = Vector3DotProduct(d, d);
    float b = Vector3DotProduct(m, d);
    if (a <= 1e-8f || b >= 0.0f) return -1.0f;
    float disc = b * b - a * cc;
    if (disc < 0.0f) return -1.0f;
    float t = (-b - sqrtf(disc)) / a;
    return (t >= 0.0f && t <= 1.0f) ? t : -1.0f;
}
} // namespace

void Shooter::Reset(const Chain& chain, unsigned seed) {
    rng_.seed(seed);
    numColors_ = chain.Config().numColors;
    yaw_ = 0.0f;
    pitch_ = -0.25f;
    cooldown_ = 0.0f;
    shots_.clear();
    current_ = PickColor(chain);
    next_ = PickColor(chain);
}

void Shooter::Look(Vector2 md) {
    yaw_ -= md.x * cfg::MOUSE_SENS;
    pitch_ -= md.y * cfg::MOUSE_SENS;
    const float lim = 85.0f * DEG2RAD;
    pitch_ = std::clamp(pitch_, -lim, lim);
}

Vector3 Shooter::Forward() const {
    return Vector3{cosf(pitch_) * sinf(yaw_), sinf(pitch_), cosf(pitch_) * cosf(yaw_)};
}
Vector3 Shooter::Right() const {
    return Vector3Normalize(Vector3CrossProduct(Forward(), Vector3{0, 1, 0}));
}
Vector3 Shooter::Up() const {
    return Vector3CrossProduct(Right(), Forward());
}

Camera3D Shooter::MakeCamera() const {
    Camera3D cam{};
    cam.position = Vector3{0, 0, 0};
    cam.target = Forward();
    cam.up = Vector3{0, 1, 0};
    cam.fovy = cfg::FOV_Y;
    cam.projection = CAMERA_PERSPECTIVE;
    return cam;
}

int Shooter::PickColor(const Chain& chain) {
    present_ = chain.ColorsPresent();
    if (present_.empty()) {
        std::uniform_int_distribution<int> d(0, numColors_ - 1);
        return d(rng_);
    }
    std::uniform_int_distribution<size_t> d(0, present_.size() - 1);
    return present_[d(rng_)];
}

void Shooter::RefreshColors(const Chain& chain) {
    present_ = chain.ColorsPresent();
    if (present_.empty()) return;
    auto has = [&](int c) { return std::find(present_.begin(), present_.end(), c) != present_.end(); };
    if (!has(current_)) current_ = PickColor(chain);
    if (!has(next_)) next_ = PickColor(chain);
}

bool Shooter::TryFire(const Chain& chain) {
    if (cooldown_ > 0.0f) return false;
    Vector3 f = Forward();
    shots_.push_back({Vector3Scale(f, 1.0f), f, current_});
    current_ = next_;
    next_ = PickColor(chain);
    cooldown_ = cfg::SHOT_COOLDOWN;
    return true;
}

void Shooter::Swap() { std::swap(current_, next_); }

void Shooter::Update(float dt, Chain& chain, const Track& track) {
    cooldown_ = std::max(0.0f, cooldown_ - dt);
    RefreshColors(chain);

    const float hitR = 2.0f * cfg::BALL_R;
    for (size_t k = 0; k < shots_.size();) {
        Projectile& p = shots_[k];
        Vector3 p1 = Vector3Add(p.pos, Vector3Scale(p.dir, cfg::SHOT_SPEED * dt));

        const auto& balls = chain.Balls();
        int bestIdx = -1;
        float bestT = 2.0f;
        for (size_t i = 0; i < balls.size(); ++i) {
            if (balls[i].state == Ball::Popping) continue;
            float t = SweepSphere(p.pos, p1, track.PosAt(balls[i].s), hitR);
            if (t >= 0.0f && t < bestT) { bestT = t; bestIdx = (int)i; }
        }

        if (bestIdx >= 0) {
            Vector3 hitPt = Vector3Lerp(p.pos, p1, bestT);
            float s = balls[bestIdx].s;
            Vector3 off = Vector3Subtract(hitPt, track.PosAt(s));
            bool towardHead = Vector3DotProduct(off, track.TangentAt(s)) > 0.0f;
            chain.Insert(bestIdx, towardHead, p.color);
            shots_.erase(shots_.begin() + k);
            continue;
        }

        p.pos = p1;
        if (Vector3Length(p.pos) > track.Radius() + 3.0f) {
            chain.OnMiss();
            shots_.erase(shots_.begin() + k);
            continue;
        }
        ++k;
    }
}
