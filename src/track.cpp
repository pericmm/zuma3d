#include "track.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

void Track::Build(float radius, float elevStartDeg, float elevEndDeg, float turns, int samples) {
    radius_ = radius;
    pts_.clear();
    cum_.clear();
    pts_.reserve(samples + 1);
    cum_.reserve(samples + 1);

    const float e0 = elevStartDeg * DEG2RAD;
    const float e1 = elevEndDeg * DEG2RAD;
    for (int i = 0; i <= samples; ++i) {
        float t   = (float)i / (float)samples;
        float phi = t * turns * 2.0f * PI;
        float e   = e0 + (e1 - e0) * t;
        Vector3 p = { radius * cosf(e) * sinf(phi), radius * sinf(e), radius * cosf(e) * cosf(phi) };
        cum_.push_back(pts_.empty() ? 0.0f : cum_.back() + Vector3Distance(pts_.back(), p));
        pts_.push_back(p);
    }
}

Vector3 Track::PosAt(float s) const {
    if (pts_.empty()) return Vector3{0, 0, 0};
    if (s <= 0.0f) return pts_.front();
    if (s >= Length()) return pts_.back();
    auto it = std::upper_bound(cum_.begin(), cum_.end(), s);
    size_t i = (size_t)(it - cum_.begin()) - 1;
    float seg = cum_[i + 1] - cum_[i];
    float f = seg > 0.0f ? (s - cum_[i]) / seg : 0.0f;
    Vector3 p = Vector3Lerp(pts_[i], pts_[i + 1], f);
    return Vector3Scale(Vector3Normalize(p), radius_);
}

Vector3 Track::TangentAt(float s) const {
    const float h = 0.05f;
    float a = std::max(0.0f, s - h);
    float b = std::min(Length(), s + h);
    return Vector3Normalize(Vector3Subtract(PosAt(b), PosAt(a)));
}
