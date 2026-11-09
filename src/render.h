#pragma once
#include <string>
#include <vector>
#include "raylib.h"

class Chain;
class Shooter;
class Track;

Color BallColor(int c);

class Renderer {
public:
    void Init(const Track& track);
    void Unload();
    void Update(float dt);
    void AddPopup(Vector3 worldPos, const std::string& text, Color col);
    void Draw(const Track& track, const Chain& chain, const Shooter& shooter, float time);

private:
    struct Popup { Vector3 pos; std::string text; Color col; float t; };

    void DrawWorld(const Track& track, const Chain& chain, const Shooter& shooter, float time);
    void DrawBall(Vector3 pos, float radius, Color col);
    void DrawHud(const Track& track, const Chain& chain, const Shooter& shooter, const Camera3D& cam);
    void DrawHeadIndicator(const Track& track, const Chain& chain, const Shooter& shooter, const Camera3D& cam);

    Shader shader_{};
    Model  sphere_{};
    int    locLight_ = -1;
    std::vector<Vector3> railA_, railB_, ties_, grid_;
    std::vector<Popup> popups_;
};
