// Headless tests for the chain logic and track geometry: `spherechain --selftest`
#include "chain.h"
#include "config.h"
#include "track.h"
#include "raymath.h"
#include <cmath>
#include <cstdio>
#include <set>

namespace {
int g_fail = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) { std::printf("    FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_fail; } \
    } while (0)

ChainConfig TestConfig() {
    ChainConfig c;
    c.trackLength = 100.0f;
    c.spacing = 1.2f;
    c.speed = 0.0f;       // static chain unless a test wants movement
    c.introCount = 0;
    c.totalBalls = 0;     // no spawning
    c.numColors = 4;
    return c;
}

void Run(Chain& ch, float seconds) {
    const float dt = 1.0f / 60.0f;
    for (float t = 0; t < seconds; t += dt) ch.Update(dt);
}

std::vector<std::pair<float, int>> Touching(float headS, std::initializer_list<int> colors) {
    std::vector<std::pair<float, int>> v;
    float s = headS;
    for (int c : colors) { v.push_back({s, c}); s -= 1.2f; }
    return v;
}

void TestInsertSpacing() {
    std::printf("  insert keeps spacing\n");
    Chain ch;
    ch.Reset(TestConfig());
    ch.SetBallsForTest(Touching(20, {0, 1, 2, 3}));
    ch.Insert(1, true, 1);
    Run(ch, 1.0f);
    const auto& b = ch.Balls();
    CHECK(b.size() == 5);
    for (size_t i = 0; i + 1 < b.size(); ++i) CHECK(b[i].s - b[i + 1].s >= 1.2f - 1e-3f);
    CHECK(b[1].color == 1 && b[2].color == 1);
}

void TestThreeMatchAndGapStays() {
    std::printf("  3-match pops; mismatched gap stays put\n");
    Chain ch;
    ch.Reset(TestConfig());
    ch.SetBallsForTest(Touching(20, {0, 1, 2, 2, 3}));
    ch.Insert(3, false, 2);   // behind the second 2 -> 2,2,2
    Run(ch, 1.0f);
    const auto& b = ch.Balls();
    CHECK(b.size() == 3);
    if (b.size() == 3) {
        CHECK(b[0].color == 0 && b[1].color == 1 && b[2].color == 3);
        float frontS = b[1].s;
        Run(ch, 2.0f);
        CHECK(std::fabs(ch.Balls()[1].s - frontS) < 1e-4f);
        CHECK(ch.Balls()[1].s - ch.Balls()[2].s > 1.2f + 0.5f);
    }
    CHECK(ch.Score() == 30);
}

void TestGapCloseCombo() {
    std::printf("  matching gap rolls back and pops (combo)\n");
    Chain ch;
    ch.Reset(TestConfig());
    ch.SetBallsForTest(Touching(10, {3, 1, 1, 0, 0, 1, 2}));
    ch.Insert(4, false, 0);   // 0,0,0 pops, then 1,1 | 1 roll together
    Run(ch, 3.0f);
    const auto& b = ch.Balls();
    CHECK(b.size() == 2);
    std::set<int> colors;
    for (auto& x : b) colors.insert(x.color);
    CHECK(colors == (std::set<int>{3, 2}));
    CHECK(ch.Score() == 30 + 30 + 100);
}

void TestLose() {
    std::printf("  lose when the head reaches the pit\n");
    ChainConfig c = TestConfig();
    c.speed = 1.0f;
    Chain ch;
    ch.Reset(c);
    ch.SetBallsForTest({{c.trackLength - 0.5f, 0}});
    Run(ch, 1.0f);
    CHECK(ch.Lost());
    CHECK(!ch.Won());
}

void TestWin() {
    std::printf("  win when the chain is empty and all balls spawned\n");
    Chain ch;
    ch.Reset(TestConfig());
    ch.SetBallsForTest(Touching(10, {0, 0}));
    ch.Insert(1, false, 0);
    Run(ch, 1.0f);
    CHECK(ch.Balls().empty());
    CHECK(ch.Won());
}

void TestSpawnAndRun() {
    std::printf("  full config spawns, keeps order, and eventually loses untouched\n");
    ChainConfig c;
    c.trackLength = 120.0f;
    c.totalBalls = 50;
    c.introCount = 20;
    Chain ch;
    ch.Reset(c);
    bool orderOk = true;
    for (int i = 0; i < 60 * 300 && !ch.Lost(); ++i) {
        ch.Update(1.0f / 60.0f);
        const auto& b = ch.Balls();
        for (size_t k = 0; k + 1 < b.size(); ++k)
            if (b[k].s - b[k + 1].s < c.spacing - 1e-3f) orderOk = false;
    }
    CHECK(orderOk);
    CHECK(ch.Lost());
}

void TestTrack() {
    std::printf("  track: monotonic arc length, loops never touch\n");
    Track t;
    t.Build(cfg::SPHERE_R, cfg::TRACK_ELEV_START, cfg::TRACK_ELEV_END, cfg::TRACK_TURNS, cfg::TRACK_SAMPLES);
    const auto& cum = t.Cum();
    for (size_t i = 1; i < cum.size(); ++i) CHECK(cum[i] > cum[i - 1]);
    std::printf("    length = %.1f units (~%d ball slots)\n", t.Length(), (int)(t.Length() / cfg::SPACING));

    const auto& p = t.Points();
    float minDist = 1e9f;
    for (size_t i = 0; i < p.size(); i += 20)
        for (size_t j = i + 20; j < p.size(); j += 20)
            if (cum[j] - cum[i] > 4.0f * cfg::SPACING) minDist = std::fmin(minDist, Vector3Distance(p[i], p[j]));
    std::printf("    min distance between non-adjacent track points = %.2f\n", minDist);
    CHECK(minDist > 2.0f * cfg::SPACING);
}
} // namespace

int RunSelfTests() {
    std::printf("selftest\n");
    TestInsertSpacing();
    TestThreeMatchAndGapStays();
    TestGapCloseCombo();
    TestLose();
    TestWin();
    TestSpawnAndRun();
    TestTrack();
    std::printf(g_fail ? "%d check(s) FAILED\n" : "all checks passed\n", g_fail);
    return g_fail ? 1 : 0;
}
