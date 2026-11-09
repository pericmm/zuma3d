#pragma once
// All gameplay tuning lives here.

namespace cfg {

// World
constexpr float SPHERE_R  = 20.0f;              // distance of the track from the player
constexpr float BALL_R    = 0.6f;
constexpr float SPACING   = 2.0f * BALL_R;      // centre-to-centre distance of touching balls

// Track: spherical spiral around the player
constexpr float TRACK_ELEV_START = -35.0f;      // degrees
constexpr float TRACK_ELEV_END   =  35.0f;      // degrees
constexpr float TRACK_TURNS      =  3.0f;
constexpr int   TRACK_SAMPLES    = 20000;

// Chain
constexpr int   TOTAL_BALLS     = 120;
constexpr int   NUM_COLORS      = 4;            // up to 6
constexpr int   INTRO_COUNT     = 30;           // balls that roll in fast at the start
constexpr float CHAIN_SPEED     = 1.6f;         // units / second
constexpr float INTRO_SPEED     = 14.0f;
constexpr float SLOW_START      = 0.80f;        // fraction of track where the chain slows
constexpr float SLOW_FACTOR     = 0.5f;
constexpr float CHAIN_ACCEL     = 30.0f;
constexpr float ROLLBACK_ACCEL  = 18.0f;
constexpr float ROLLBACK_MAX    = 14.0f;
constexpr float STOP_DECEL      = 30.0f;
constexpr float INSERT_TIME     = 0.15f;        // seconds for an inserted ball to open its slot
constexpr float POP_TIME        = 0.25f;
constexpr float DRAIN_SPEED     = 40.0f;        // chain speed after losing

// Shooter
constexpr float SHOT_SPEED    = 60.0f;
constexpr float SHOT_COOLDOWN = 0.18f;
constexpr float MOUSE_SENS    = 0.0025f;        // radians per pixel
constexpr float FOV_Y         = 70.0f;

} // namespace cfg
