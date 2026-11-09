#include "chain.h"
#include <algorithm>
#include <set>
#include <unordered_set>

namespace {
constexpr float kTouchEps = 0.02f;   // fraction of spacing tolerated as "touching"

float Approach(float v, float target, float step) {
    return v < target ? std::min(v + step, target) : std::max(v - step, target);
}
uint64_t PairKey(int a, int b) { return (uint64_t(uint32_t(a)) << 32) | uint32_t(b); }
} // namespace

void Chain::Reset(const ChainConfig& c) {
    cfg_ = c;
    balls_.clear();
    events_.clear();
    rng_.seed(c.seed);
    nextId_ = 1;
    spawned_ = score_ = streak_ = comboDepth_ = 0;
    last_ = last2_ = -1;
    lost_ = won_ = false;
}

void Chain::SetBallsForTest(const std::vector<std::pair<float, int>>& v) {
    balls_.clear();
    for (auto& [s, c] : v) {
        Ball b;
        b.id = nextId_++;
        b.s = s;
        b.color = c;
        balls_.push_back(b);
    }
}

float Chain::Req(size_t i) const {
    return cfg_.spacing * 0.5f * (balls_[i].w + balls_[i + 1].w);
}

bool Chain::Touching(size_t i) const {
    return balls_[i].s - balls_[i + 1].s <= Req(i) + cfg_.spacing * kTouchEps;
}

float Chain::HeadProgress() const {
    if (balls_.empty() || cfg_.trackLength <= 0) return 0.0f;
    return std::clamp(balls_.front().s / cfg_.trackLength, 0.0f, 1.0f);
}

std::vector<int> Chain::ColorsPresent() const {
    std::set<int> s;
    for (auto& b : balls_)
        if (b.state != Ball::Popping) s.insert(b.color);
    return {s.begin(), s.end()};
}

std::vector<ChainEvent> Chain::TakeEvents() {
    std::vector<ChainEvent> out;
    out.swap(events_);
    return out;
}

int Chain::PickColor() {
    std::uniform_int_distribution<int> d(0, cfg_.numColors - 1);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    int c;
    if (last_ >= 0 && last_ == last2_ && cfg_.numColors > 1) {
        do { c = d(rng_); } while (c == last_);     // never spawn 3 in a row
    } else if (last_ >= 0 && u(rng_) < 0.45f) {
        c = last_;                                   // bias toward pairs
    } else {
        c = d(rng_);
    }
    last2_ = last_;
    last_ = c;
    return c;
}

void Chain::Spawn() {
    while (spawned_ < cfg_.totalBalls) {
        float s = 0.0f;
        float vel = 0.0f;
        if (!balls_.empty()) {
            float bs = balls_.back().s;
            if (bs < cfg_.spacing) break;
            // Stay in contact with the rear ball if it is just past the entrance,
            // otherwise emerge from the entrance as a new pushing segment.
            s = (bs < 2.0f * cfg_.spacing) ? bs - cfg_.spacing : 0.0f;
            vel = balls_.back().vel;
        }
        Ball b;
        b.id = nextId_++;
        b.s = s;
        b.vel = vel;
        b.color = PickColor();
        balls_.push_back(b);
        ++spawned_;
    }
}

void Chain::Insert(int hitIdx, bool towardHead, int color) {
    if (hitIdx < 0 || hitIdx >= (int)balls_.size()) return;
    const Ball& h = balls_[hitIdx];
    Ball nb;
    nb.id = nextId_++;
    nb.color = color;
    nb.state = Ball::Inserting;
    nb.w = 0.0f;
    nb.vel = h.vel;
    nb.fromShot = true;
    size_t idx;
    if (towardHead) { nb.s = h.s + cfg_.spacing * 0.5f; idx = hitIdx; }
    else            { nb.s = h.s - cfg_.spacing * 0.5f; idx = hitIdx + 1; }
    balls_.insert(balls_.begin() + idx, nb);
    events_.push_back({ChainEvent::Inserted, nb.s});
}

void Chain::CheckMatch(size_t i) {
    Ball& b = balls_[i];
    bool gapClose = b.pendingGapClose;
    bool fromShot = b.fromShot;
    b.pendingCheck = b.pendingGapClose = b.fromShot = false;
    if (b.state != Ball::Normal) return;

    const int c = b.color;
    size_t lo = i, hi = i;
    while (lo > 0 && Touching(lo - 1) && balls_[lo - 1].color == c && balls_[lo - 1].state == Ball::Normal) --lo;
    while (hi + 1 < balls_.size() && Touching(hi) && balls_[hi + 1].color == c && balls_[hi + 1].state == Ball::Normal) ++hi;
    int count = int(hi - lo + 1);

    if (count >= 3) {
        for (size_t k = lo; k <= hi; ++k) {
            balls_[k].state = Ball::Popping;
            balls_[k].anim = 0.0f;
            balls_[k].pendingCheck = false;
        }
        int points = 10 * count;
        if (gapClose) {
            ++comboDepth_;
            points += 100 * comboDepth_;
        } else if (fromShot) {
            comboDepth_ = 0;
            ++streak_;
            if (streak_ >= 5) points += 100 + (streak_ - 5) * 10;
        }
        score_ += points;
        ChainEvent e{ChainEvent::Pop, balls_[(lo + hi) / 2].s};
        e.count = count;
        e.combo = gapClose ? comboDepth_ : 0;
        e.streak = streak_;
        e.points = points;
        events_.push_back(e);
    } else {
        if (fromShot) {
            streak_ = 0;
            events_.push_back({ChainEvent::ShotNoMatch, b.s});
        }
        if (gapClose) comboDepth_ = 0;
    }
}

void Chain::CheckEnd() {
    if (lost_ || won_) return;
    if (!balls_.empty() && balls_.front().s >= cfg_.trackLength) {
        lost_ = true;
        streak_ = 0;
        events_.push_back({ChainEvent::Lost, cfg_.trackLength});
    } else if (spawned_ >= cfg_.totalBalls && balls_.empty()) {
        won_ = true;
        events_.push_back({ChainEvent::Won, 0.0f});
    }
}

void Chain::Update(float dt) {
    if (won_) return;

    auto finishedPop = [](const Ball& b) { return b.state == Ball::Popping && b.anim >= 1.0f; };

    if (lost_) {   // everything drains into the pit
        for (auto& b : balls_) {
            b.s += cfg_.drainSpeed * dt;
            if (b.state == Ball::Popping) b.anim += dt / cfg_.popTime;
        }
        const float L = cfg_.trackLength;
        balls_.erase(std::remove_if(balls_.begin(), balls_.end(),
                                    [&](const Ball& b) { return b.s >= L || finishedPop(b); }),
                     balls_.end());
        return;
    }

    // 1. Animations
    for (auto& b : balls_) {
        if (b.state == Ball::Inserting) {
            b.w += dt / cfg_.insertTime;
            if (b.w >= 1.0f) {
                b.w = 1.0f;
                b.state = Ball::Normal;
                b.pendingCheck = true;
            }
        } else if (b.state == Ball::Popping) {
            b.anim += dt / cfg_.popTime;
        }
    }
    balls_.erase(std::remove_if(balls_.begin(), balls_.end(), finishedPop), balls_.end());

    // 2. New balls from the entrance
    Spawn();
    if (balls_.empty()) { CheckEnd(); return; }

    // 3. Segments and their velocities
    std::unordered_set<uint64_t> breaks;
    std::vector<std::pair<size_t, size_t>> segs;
    size_t start = 0;
    for (size_t i = 0; i + 1 < balls_.size(); ++i) {
        if (!Touching(i)) {
            segs.push_back({start, i});
            breaks.insert(PairKey(balls_[i].id, balls_[i + 1].id));
            start = i + 1;
        }
    }
    segs.push_back({start, balls_.size() - 1});

    float rearSpeed = spawned_ < cfg_.introCount ? cfg_.introSpeed : cfg_.speed;
    if (balls_[segs.back().first].s > cfg_.slowStart * cfg_.trackLength) rearSpeed *= cfg_.slowFactor;

    for (size_t k = 0; k < segs.size(); ++k) {
        auto [a, e] = segs[k];
        float v = balls_[e].vel;
        if (k + 1 == segs.size()) {
            v = Approach(v, rearSpeed, cfg_.accel * dt);                 // pushed from the entrance
        } else {
            const Ball& r = balls_[e];
            const Ball& nx = balls_[e + 1];
            bool attract = r.color == nx.color && r.state != Ball::Popping && nx.state != Ball::Popping;
            if (attract) v = std::max(v - cfg_.rollbackAccel * dt, -cfg_.rollbackMax);
            else         v = Approach(v, 0.0f, cfg_.stopDecel * dt);     // detached: stop
        }
        for (size_t i = a; i <= e; ++i) {
            balls_[i].vel = v;
            balls_[i].s += v * dt;
        }
    }

    // 4. Resolve overlaps from the back: contact pushes balls ahead forward
    for (size_t i = balls_.size() - 1; i-- > 0;) {
        float need = balls_[i + 1].s + Req(i);
        if (balls_[i].s < need) balls_[i].s = need;
    }

    // 5. Gaps that just closed between same-coloured balls trigger a match check
    for (size_t i = 0; i + 1 < balls_.size(); ++i) {
        if (Touching(i) && breaks.count(PairKey(balls_[i].id, balls_[i + 1].id)) &&
            balls_[i].color == balls_[i + 1].color &&
            balls_[i].state == Ball::Normal && balls_[i + 1].state == Ball::Normal) {
            balls_[i + 1].pendingCheck = true;
            balls_[i + 1].pendingGapClose = true;
        }
    }

    // 6. Match checks (only mark balls, so indices stay valid)
    for (size_t i = 0; i < balls_.size(); ++i)
        if (balls_[i].pendingCheck) CheckMatch(i);

    // 7. Win / lose
    CheckEnd();
}
