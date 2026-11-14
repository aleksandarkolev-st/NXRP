# Brief audit - 2026-11-08

The attachment's original scope is retained. The implementation below is
packaged for review; the complete competition-quality goal is not yet proven.

## Requirement evidence

| Original requirement | Current implementation and evidence | Assessment |
| --- | --- | --- |
| Better slots and poker | Sourced slot cabinet/lever/reels; staged pull, sequential reels, distinct sounds and jackpot. Keeper's Draw supports holds, one replacement draw and individual co-op scores. CoopSmoke exhausts 2,598,960 hands and tests every hold mask across 100 seeds; shared slot/poker actions and GPU renders pass. | Implemented and automatically verified |
| Sourced skeletal hands, IK, fingers and weighted throws | HandRig.h solves upper/lower arm IK and skins original source bones/weights, including finger curl. GameView uses reach, inertia, grip, wind-up and follow-through. Rig loading, singular/unreachable IK, wrist targets, delayed release and physical scoring pass in CoopSmoke; held, wind-up, release, lever and drink frames were rendered. Source records ship under assets/models. | Implemented; subjective animation feel still needs a player |
| Roughness, normals, softer shadows, wet reflections, restrained fog | world.frag consumes source OpenGL normal/ARM maps; post.frag uses roughness-dependent screen-space reflections, four shadow samples and bounded fog. The GPU smoke loads maps, requires shader compilation, verifies a mapped/unmapped pixel difference and renders a hunter sightline. Screen-space tracing reports active. | Implemented within screen-space rendering limits |
| Monster breathing, uneven steps, knocks, positional growls, muffling and silence | Maze.Soundscape.cpp layers breathing, irregular footfalls, growls/roars and knocks. Maze.Acoustics.h supplies wall/door attenuation; GameView applies occlusion and occasional ambience hush. Native tests cover positional sound, attenuation, pursuit vocals and pause fade. | Implemented; subjective headphone listening still needs a player |
| Fair evidence-driven pursuit, lost trails, door lingering and learned escapes | Maze.Hunter.cpp stores observed targets, last known locations, search/linger state and observed junction exits. Tests prove hidden movement cannot steer remembered search, lost trails expire, only observed routes are learned, and floors/doors block sight. Actual doorway pursuit in the TCP fixture delivers two warnings and forces the third strike. | Implemented and automatically verified |
| Abandoned belongings, damaged shelters, footprints, scratched signs and connected history | Downloaded luggage/toolbox/recorder/cabin models, four districts, linked notes and route-aligned footprint decals. Placement/connectivity tests pass; source meshes/textures load on GPU; story and ruin renders were inspected. Sign UV inversion was corrected. Credits are in STORY_SOURCES.md. | Implemented and rendered |
| Noisy relief, crouching, running/breath, temporary houses with exits | Bar/slots/poker create audible-only hunter evidence. Drinks restore breath/reduce fear; crouching reduces detection and speed; running consumes breath. Houses have four doors, checkpoints and physical stairs. The creature enters, climbs, searches and captures. Tests cover individual captures, door collision, stairs, sports and shared world state. | Implemented and automatically verified |
| Dynamic passages, jumping and crouching | Seeded irregular opening/closing delays, occupancy protection, synchronized shift state, jump arc/landing and crouch stance. TCP smoke checks safe shifts and replicated stances; native connectivity tests span all supported dimensions. | Implemented and automatically verified |
| Voice chat | Hold-V transient capture, compressed PCM packets, host authentication/rate limits, distance culling, per-listener pan/occlusion, local mute and focus/pause cleanup. The real TCP fixture verifies relay, no self-echo, replay rejection, generation checks, range culling and speech noise. Windows reports working audio and microphone endpoints. | Network path verified; physical microphone-to-headphone and internet sessions remain unverified |
| Fun, eerie, competition-quality overall experience | Story/refuge contrast, risk-bearing games, fear effects, audible pursuit, shifting routes and readable door warnings. The new five-page Keeper's Field Guide explains all systems at 640x560 without shrinking below 18px. | Implemented elements are verified; overall pacing/scare quality requires human playtesting |

## Current validation and deliverable

- Release/x64 solution build passes using installed v143.
- Latest native test execution: 150/150 pass.
- Latest real four-player loopback TCP/OpenGL smoke: pass.
- `CoopSmoke.exe --guide`: all five pages fit at 640x560; keyboard, pointer,
  boundary states and Back navigation pass. All five renders were generated;
  survival and co-op pages were visually inspected.
- Extracted package: all 94 manifest entries match SHA256.
- `git diff --check`: no whitespace errors (line-ending notices only).

Archive: `build/packages/TheHedge-FieldGuide-Windows-x64.zip`

SHA256: `3B5C99DCC4FECAA235B378EAE704057B6892DF1AE5673DCF20D3FA3CB9010705`

## Remaining gate and blocked audit

The Keeper's Draw checkpoint, Third Knock checkpoint and this audit all leave
the same external gate unresolved: physical-device/internet co-op evaluation
and a human judgement of horror pacing and interaction feel. Local automated
checks cannot establish those claims. The Windows Computer Use helper again
returns `native pipe is unavailable` (OS error 2), so live desktop inspection
is unavailable in this session. GPU render verification remains available and
has been used; it is not described as a human playtest.

No known missing feature from the attachment remains in the implementation.
The goal must remain unachieved until that external evaluation supplies evidence
or identifies issues to fix. Adding unrelated features would not prove this
remaining gate. A useful session is two players with headphones: check hold-V
in both directions, speech through a closed door, a lost-trail escape, the two
strike warnings, and a noisy refuge game. Record any issue and its maze seed.
