#pragma once
#include "raylib.h"

// Small procedurally generated sound effects (no asset files).
class Sounds {
public:
    void Load();
    void Unload();
    void Shoot();
    void Click();
    void Pop(int combo);
    void Lose();
    void Win();

private:
    bool  ok_ = false;
    Sound shoot_{}, click_{}, pop_{}, lose_{}, win_{};
};
