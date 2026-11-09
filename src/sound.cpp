#include "sound.h"
#include <cmath>
#include <cstdlib>

namespace {
// Frequency sweep with exponential decay and optional noise.
Sound MakeTone(float f0, float f1, float dur, float noise, float decay, float vol) {
    const int sr = 44100;
    const unsigned frames = (unsigned)(dur * sr);
    short* data = (short*)MemAlloc(frames * sizeof(short));
    float phase = 0.0f;
    for (unsigned i = 0; i < frames; ++i) {
        float t = (float)i / sr;
        float f = f0 + (f1 - f0) * (t / dur);
        phase += 2.0f * PI * f / sr;
        float env = expf(-t * decay) * fminf(1.0f, i / (0.003f * sr));
        float n = (float)rand() / RAND_MAX * 2.0f - 1.0f;
        float v = (sinf(phase) * (1.0f - noise) + n * noise) * env * vol;
        data[i] = (short)(fmaxf(-1.0f, fminf(1.0f, v)) * 32767.0f);
    }
    Wave w{};
    w.frameCount = frames;
    w.sampleRate = sr;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = data;
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}
} // namespace

void Sounds::Load() {
    InitAudioDevice();
    ok_ = IsAudioDeviceReady();
    if (!ok_) return;
    shoot_ = MakeTone(700, 250, 0.12f, 0.15f, 25, 0.5f);
    click_ = MakeTone(1800, 1100, 0.05f, 0.5f, 70, 0.5f);
    pop_   = MakeTone(450, 950, 0.20f, 0.10f, 14, 0.55f);
    lose_  = MakeTone(300, 60, 1.20f, 0.20f, 2, 0.6f);
    win_   = MakeTone(500, 1200, 0.80f, 0.00f, 3, 0.5f);
}

void Sounds::Unload() {
    if (!ok_) return;
    UnloadSound(shoot_); UnloadSound(click_); UnloadSound(pop_);
    UnloadSound(lose_);  UnloadSound(win_);
    CloseAudioDevice();
}

void Sounds::Shoot() { if (ok_) PlaySound(shoot_); }
void Sounds::Click() { if (ok_) PlaySound(click_); }
void Sounds::Pop(int combo) {
    if (!ok_) return;
    SetSoundPitch(pop_, 1.0f + 0.15f * combo);
    PlaySound(pop_);
}
void Sounds::Lose() { if (ok_) PlaySound(lose_); }
void Sounds::Win()  { if (ok_) PlaySound(win_); }
