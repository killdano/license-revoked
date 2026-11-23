# CLAUDE.md — License Revoked

A GTA 1-style free-roam game for the Pebble Time 2 (emery), plus an
Android port of the same game code. This repo is **public**: never put
keys, tokens, access codes or anything private in it.

- `src/c/` — the game (watch). `tools/hostcheck/` runs it headless
  (`make check`, `make fuzz`). `DESIGN.md` and `README.md` hold the design
  history; `HANDOFF.md` the latest state.
- `android/` — the phone build: the same C code under `WR_ANDROID`, plus a
  platform layer. `README-android.md` explains it. The watch build is
  untouched by anything phone-side.

## Builds

- `.github/workflows/build.yml` builds the `.pbw` on every push. Test
  builds from `claude/` branches are stamped `9.N.0`.
- `.github/workflows/android.yml` builds the debug APK; on `main` it
  replaces the APK on the `android` release, so this link is always the
  newest: https://github.com/killdano/license-revoked/releases/download/android/LicenseRevoked-android.apk
- `.github/workflows/release.yml` publishes to the Pebble appstore (run by
  hand from the Actions tab; needs the `PEBBLE_LOGIN` secret). Its release
  notes always end with the Android link. Don't bump `version` in
  `package.json` yourself; the release sets it.

## Rules

- Dan doesn't code; he plays on a real Pebble Time 2 and a Galaxy Fold.
- Reference GTA 1 for any gameplay question he hasn't answered.
- Keep the watch build within its memory limits; measure, don't guess.
- A clean build is not a tested game: say what you couldn't check.
- Showing work on Dan's status page (Dan, 2026-11-23): if your session has
  the MirrorApps desk connector, call its `crew_busy` tool when you start
  work here (app `license-revoked`, or `license-revoked-android` for the
  phone build; `who` as Dan would say it, e.g. "Claude in chat"), again
  every half hour or so, and with `done: true` when you stop. Without the
  connector, skip it.
