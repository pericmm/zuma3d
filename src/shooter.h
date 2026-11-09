#pragma once
#include <random>
#include <vector>
#include "raylib.h"

class Chain;
class Track;

struct Projectile {
    Vector3 pos;
    Vector3 dir;
    int     color;
};

// First-person shooter standing at the origin.
class Shooter {
public:
    void Reset(const Chain& chain, unsigned seed);
    void Look(Vector2 mouseDelta);

    Vector3  Forward() const;
    Vector3  Right() const;      // screen right
    Vector3  Up() const;         // screen up
    Camera3D MakeCamera() const;

    bool TryFire(const Chain& chain);              // returns true if a ball was fired
    void Swap();
    void Update(float dt, Chain& chain, const Track& track);

    int Current() const { return current_; }
    int Next() const { return next_; }
    const std::vector<Projectile>& Shots() const { return shots_; }

private:
    int  PickColor(const Chain& chain);
    void RefreshColors(const Chain& chain);

    float yaw_ = 0.0f;
    float pitch_ = -0.25f;
    int   current_ = 0, next_ = 0;
    float cooldown_ = 0.0f;
    std::vector<Projectile> shots_;
    std::mt19937 rng_;
    std::vector<int> present_;
    int numColors_ = 4;
};
