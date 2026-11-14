# Third Knock pass - 2026-11-08

## Changes

- Co-op protocol 15 carries the authoritative door event counter, event source,
  impact type, rattle amplitude and each door's strike count. Snapshot parsing
  validates those values before changing the live world.
- Guests now see the same rattles and receive the same impact events as the
  host. Door sound pans from the actual door relative to each listener and
  fades with distance. Distant events no longer sound as loud as nearby ones.
- Nearby threats show a left/right/ahead/behind warning. The first and second
  strikes are distinct; a forced door has an urgent warning. This panel stays
  clear of the normal refuge prompts at the minimum supported window size.
- Captures rebase slot stop times and poker cooldown times into the restarted
  encounter clock. Remaining animation durations, cards and scores survive.
- Poker cards now slide and flip in sequence. Held cards stay face-up during
  the draw and lift above unheld cards.

## Evidence

Release/x64 builds successfully using the installed v143 toolset. Native unit
tests pass 150/150, including the new distant-door silence/ambience regression.
The real four-player loopback smoke passes, including first/second-strike
replication, opposite audio pans for listeners on opposite sides, third-hit
forced entry, capture timer preservation, poker rankings and previous systems.

Actual OpenGL renders were inspected at 640x560:
`build/door-warning.png`, `build/poker-dealing.png`, `build/refuge-poker.png`.
Desktop Computer Use inspection was attempted, retried and reset once; the
native helper returned `native pipe is unavailable` (OS error 2). Thus these
are rendered-game checks, not an interactive human playtest.

Archive: `build/packages/TheHedge-ThirdKnock-Windows-x64.zip`

SHA256: `7DD11C5754478C48260284CBE7C16EC6FFEEBC07D606377AB3186ECFE5A7F9B3`

All 94 file entries in the extracted archive match the manifest. VERSION
identifies protocol 15 and explicitly records uncommitted source changes.
All co-op players must use this build together.

Human co-op evaluation of physical voice devices, internet latency and overall
scare pacing remains outstanding, as recorded for the prior pass.
