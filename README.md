# Sphere Chain

A first-person 3D ball-chain shooter prototype, inspired by the classic genre. You stand at the centre of a sphere. A chain of coloured balls rolls along a spiral track that wraps around you. Look around with the mouse and shoot balls into the chain. Three or more of the same colour pop. Don't let the head of the chain reach the pit.

## Build (Linux)

```sh
sudo apt install -y build-essential cmake git libx11-dev libxrandr-dev libxinerama-dev \
    libxcursor-dev libxi-dev libgl1-mesa-dev libasound2-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/spherechain             # play
./build/spherechain --selftest  # headless logic tests
```

The first configure downloads raylib 5.5 automatically.

## Controls

| Input             | Action                          |
|-------------------|---------------------------------|
| Mouse             | Look around                     |
| Left click        | Shoot                           |
| Right click/Space | Swap current and next ball      |
| R                 | Restart                         |
| Esc               | Quit                            |

## Rules

- Balls emerge from the cyan ring and are pushed along the track toward the red pit.
- A shot ball is inserted into the chain. Three or more touching balls of the same colour pop.
- When a pop leaves a gap and the balls on both sides share a colour, the front part rolls back. If it forms a new match it pops as a **combo** (+100 per level).
- Popping on 5 or more consecutive shots earns a **streak** bonus.
- The arrow on the screen edge points to the head of the chain. It turns red when the head is close to the pit.
- You win when every ball has spawned and the chain is cleared.

## Code layout

| File              | Purpose                                                        |
|-------------------|----------------------------------------------------------------|
| `src/config.h`    | All tuning constants (speeds, sizes, colour count, ...)        |
| `src/track.*`     | Spherical spiral, arc-length lookup (`PosAt`, `TangentAt`)     |
| `src/chain.*`     | Pure chain logic: spawn, push, insert, match, roll-back, win/lose |
| `src/shooter.*`   | Mouse-look camera, projectiles, hit test and insert side       |
| `src/render.*`    | Lit balls (embedded shader), track, backdrop, HUD, indicator   |
| `src/sound.*`     | Procedurally generated sound effects                           |
| `src/selftest.cpp`| `--selftest` checks for chain and track                        |
