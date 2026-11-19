# License Revoked on Android

The phone build. The game code is exactly the watch's - every domain
linked in as the monolith, nothing added - under a platform layer that
draws the SDK's rects, circles, lines and text onto the same 200x228
8-bit framebuffer the watch uses, scaled up and centred on the display.

- Rendered at three times the watch's resolution (684 tall; the width
  follows the screen's aspect at run time, so 1:1 pixels at a uniform
  scale, and a wide screen sees more of the world rather than a stretched
  one). The projection scales via `PX()`; the character's place is the
  watch's offset from the middle, whatever the width. The simulation runs at the watch's 25 Hz;
  rendering runs at the display's rate (120 Hz on the Fold), so camera
  turns from the pad draw smoothly between simulation steps.
- Controls are the phone's own: the left third of the screen is the
  movement stick; the rest is a trackpad that turns you on foot, with
  inertia (a tap on it is the action; two fingers pinch and spread the
  zoom, and once a pinch has started single-finger turning is ignored
  until every finger lifts). One translucent ACTION button at the bottom
  right, the watch's SELECT: tap attacks, hold is the car door. Hold it
  and slide off: it stays pressed (a flamethrower keeps going) and the
  slide turns you. In a car a
  translucent steering wheel appears around it - the button is the hub:
  turn it by dragging its rim, it springs back to centre when released,
  and half lock is half the car's steering. There is no zoom slider on the
  phone (pinch instead). Android's BACK is the pause menu; touch a row to
  select it.
- Sound is the synth's stream through OpenSL ES.
- The watch build is untouched by any of this: everything phone-side is
  under `WR_ANDROID` or in `android/`.

## Building

Matches the owner's known-good toolchain (AGP 9.1.1, compileSdk 36, minSdk
30, Kotlin DSL, gradle wrapper 9.1). NDK 27 and CMake 3.22 via the SDK
manager. From PowerShell:

    cd C:\dev\license-revoked\android ; .\gradlew :app:installDebug

Wireless debugging works over the VPN: `adb connect 10.6.0.2:<port>` first.

## The device

Built for a Galaxy Z Fold: the inner screen is near-square, so the game
area scales by whichever axis is tighter and the button strip is sized
from the shorter side. A fold or unfold is a window resize, not a new
activity - the layout is redone from the window's size and the game
state, which lives in C, is untouched.

The sources are compiled straight from `../src/`, so the phone build always
matches the watch build in the same checkout.

## Known gaps in this first cut

- The HUD text uses a 5x7 bitmap font rather than the watch's Gothic.
- `WR_GOD` is off in this build (see `CMakeLists.txt`).
- Untested on a device at the time of writing: built for the first time
  by the owner.
