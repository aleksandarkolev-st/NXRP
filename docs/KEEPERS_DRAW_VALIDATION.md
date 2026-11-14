# Keeper's Draw pass — 2026-11-08

Implemented five-card draw at the existing sourced refuge bar. P deals/draws,
1–5 toggle holds, Q hides the panel. Each survivor has an independent hand and
free-play score. The host owns deals, draws and scoring; snapshots validate
card ranges, uniqueness, hand phase and timing before committing state. Co-op
protocol is 14. Deals/draws create hunter noise and directional paper/tap audio;
the world continues running while the panel is open.

Also completed validation and attribution for the current story work: four
districts, downloaded luggage/toolbox/recorder props, damaged sourced cabin,
scratched signs, linked notes and passage-aligned footprint decals. Visual QA
found and corrected inverted sign UVs in the reproducible converter.

## Verification

- Release/x64 builds with installed MSVC v143.
- 149/149 native unit tests pass, including story placement, bypasses,
  connectivity, acoustic occlusion and hunter search/route learning.
- Exhaustive enumeration of 2,598,960 poker hands matches category frequencies.
- 100 seeds × all 32 hold masks preserve held cards and produce valid draws.
- Directional card audio changes only the intended channel at full-left pan.
- Real four-player loopback smoke passes capacity, puzzles, voice packet relay,
  shared games, poker guest input/panel opening, replicated holds/results,
  malformed snapshot rejection, captures, reconnects and shared victory.
- All downloaded story meshes/textures and footprint textures load on the GPU.
- Rendered poker, notes, props, ruin and current materials inspected in PNGs
  under `build/`; screen-space lighting is active in the smoke renderer.
- Extracted package: all 94 manifest entries match SHA256.
- Packaged executable from the first archive reached the responsive game
  window and closed normally; v2 changes only the corrected sign asset.

Final archive: `build/packages/TheHedge-KeepersDraw-v2-Windows-x64.zip`

SHA256: `1B3FE3523A0A39C9CCBD51BCA51AE7239CC93D72A66BEE2D881FB91DAA192376`

## Remaining evaluation

Physical microphone-to-speaker voice quality and internet latency still need a
human co-op session. Automated verification establishes implementation behavior;
competition readiness also needs human playtesting for pacing, scares and feel.
Poker is individual five-card draw with free-play scoring, not a betting table
between survivors. Existing world geometry and legacy fallback graphics remain.
