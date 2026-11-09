#pragma once
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

// Pure gameplay logic for the rolling ball chain. Knows nothing about
// rendering: balls live at a 1D arc-length position `s` along the track.

struct Ball {
    enum State : uint8_t { Normal, Inserting, Popping };

    int   id    = 0;
    float s     = 0.0f;   // distance along the track
    int   color = 0;
    State state = Normal;
    float w     = 1.0f;   // slot width factor 0..1 (grows while inserting)
    float anim  = 0.0f;   // pop progress 0..1
    float vel   = 0.0f;
    bool  pendingCheck    = false;
    bool  pendingGapClose = false;
    bool  fromShot        = false;
};

struct ChainEvent {
    enum Type { Pop, ShotNoMatch, Inserted, Lost, Won };
    Type  type;
    float s      = 0.0f;
    int   count  = 0;
    int   combo  = 0;    // gap-close chain-reaction depth (0 = direct hit)
    int   streak = 0;    // consecutive successful shots
    int   points = 0;
};

struct ChainConfig {
    float    trackLength    = 300.0f;
    float    spacing        = 1.2f;
    float    speed          = 1.6f;
    float    introSpeed     = 14.0f;
    int      introCount     = 30;
    float    slowStart      = 0.8f;
    float    slowFactor     = 0.5f;
    float    accel          = 30.0f;
    float    rollbackAccel  = 18.0f;
    float    rollbackMax    = 14.0f;
    float    stopDecel      = 30.0f;
    float    insertTime     = 0.15f;
    float    popTime        = 0.25f;
    float    drainSpeed     = 40.0f;
    int      totalBalls     = 120;
    int      numColors      = 4;
    uint32_t seed           = 1;
};

class Chain {
public:
    void Reset(const ChainConfig& c);
    void SetBallsForTest(const std::vector<std::pair<float, int>>& sAndColor); // descending s

    void Update(float dt);

    // Insert a shot ball next to balls[hitIdx]. towardHead = on the side of larger s.
    void Insert(int hitIdx, bool towardHead, int color);
    void OnMiss() { streak_ = 0; }

    const std::vector<Ball>& Balls() const { return balls_; }
    std::vector<int> ColorsPresent() const;
    std::vector<ChainEvent> TakeEvents();

    bool  Lost()  const { return lost_; }
    bool  Won()   const { return won_; }
    int   Score() const { return score_; }
    int   Streak() const { return streak_; }
    int   Remaining() const { return cfg_.totalBalls - spawned_; }
    float HeadProgress() const;
    const ChainConfig& Config() const { return cfg_; }

private:
    float Req(size_t i) const;       // required centre gap between i and i+1
    bool  Touching(size_t i) const;  // are i and i+1 in contact?
    void  Spawn();
    int   PickColor();
    void  CheckMatch(size_t i);
    void  CheckEnd();

    ChainConfig cfg_;
    std::vector<Ball> balls_;        // index 0 = head (largest s)
    std::vector<ChainEvent> events_;
    std::mt19937 rng_;
    int  nextId_ = 1;
    int  spawned_ = 0;
    int  score_ = 0;
    int  streak_ = 0;
    int  comboDepth_ = 0;
    int  last_ = -1, last2_ = -1;
    bool lost_ = false, won_ = false;
};
