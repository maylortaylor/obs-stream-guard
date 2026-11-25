# AGENTS.md: obs-stream-guard

Goal: keep released music off a livestream, and warn when the guard cannot check.

## Hard rules

1. Never test against YouTube or any public platform. Use a recording or a local RTMP server.
2. Never commit a key, a secret, a recording or anything in `private/`.
3. Never write that the tool prevents copyright action. Write that it lowers the risk.
4. The recording must never lose audio. Mute only the stream track.
5. A detector failure never stops the stream.

## Style

Short active sentences. No em dashes. No semicolons in prose. C++17, tabs, `.clang-format`
(version 19), `gersemi` for CMake.

## Where things are

- `src/core/`: guard state machine, track check, audio window. No OBS types.
- `src/recognizer/`: vibra fingerprint and the Shazam request.
- `src/obs/`: capture, mute, stop, dock.
- `tests/` and `tools/sg_scan.cpp`: run without OBS.
- Design: `docs/PLAN.md`. Setup: `docs/windows-setup.md`. Detectors: `docs/providers.md`.
