#pragma once
#include <vector>
#include "raylib.h"

// A continuous curve on a sphere centred at the origin (the player),
// parameterised by arc length s in [0, Length()].
class Track {
public:
    void Build(float radius, float elevStartDeg, float elevEndDeg, float turns, int samples);

    float   Length() const { return cum_.empty() ? 0.0f : cum_.back(); }
    float   Radius() const { return radius_; }
    Vector3 PosAt(float s) const;
    Vector3 TangentAt(float s) const;   // unit vector pointing toward increasing s

    const std::vector<Vector3>& Points() const { return pts_; }
    const std::vector<float>&   Cum()    const { return cum_; }

private:
    float radius_ = 1.0f;
    std::vector<Vector3> pts_;
    std::vector<float>   cum_;
};
