# MappyZ

MappyZ is a modern gamepad remapping runtime. It is designed to turn physical
gamepad input into keyboard, mouse, and future extensible action outputs through a
clear runtime pipeline:

```text
Physical Gamepad -> Input Backend -> SInputEvent -> Runtime -> SAction -> Output Backend
```

The project is currently in the planning and bootstrap phase. The working technical
draft is in [docs/blueprint.md](docs/blueprint.md).

## Scope

MVP work starts on Windows with C++20, CMake, SDL3 for input, and Qt 6/QML for UI.
The first goal is a stable, observable path from physical gamepad input to keyboard
and mouse output.

## Bootstrap

```powershell
cmake -S . -B build
cmake --build build
```
