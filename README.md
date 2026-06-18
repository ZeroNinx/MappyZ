# MappyZ

MappyZ is a modern gamepad remapping runtime planned around a clean input-to-action
pipeline:

```text
Physical Gamepad -> Input Backend -> SInputEvent -> Runtime -> SAction -> Output Backend
```

The current repository state is an initial project shell. The working technical draft
is in [docs/blueprint.md](docs/blueprint.md).

## Scope

MVP work starts on Windows with C++20, CMake, SDL3 for input, and Qt 6/QML for UI.
The first goal is a stable, observable path from physical gamepad input to keyboard
and mouse output.

## Source Boundaries

All third-party source references and vendored dependencies must live under
`third_party/`.

AntiMicroZ is treated as a reference project only. MappyZ does not copy, translate,
or reimplement AntiMicroX/AntiMicroZ source structure.

## Bootstrap

```powershell
cmake -S . -B build
cmake --build build
```

