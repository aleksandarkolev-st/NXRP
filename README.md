# NXRP: The Hedge

A first-person maze game for Windows, built in C++17 with SFML and OpenGL. Explore a seeded hedge maze, solve the stone ritual and gate inscription, and reach the gate while a creature follows the passages. The hedge opens temporary passages and closes them again when they are clear. Lit safehouses offer refuge and checkpoints behind doors you can open and close. A screen-space ray-traced lighting pass adds reflections, contact shadows and light travelling through the fog, while a fear system bends the image and audio when the creature closes in.

## Play

- **W / S** move forward and back; **A / D** strafe.
- **Mouse** or **Left / Right arrows** turn; **Space** jumps and **Ctrl/C** crouches quietly; **Shift** runs while breath lasts. Breath recovers when you stop running.
- **E** opens and closes nearby safehouse doors, reads the entrance inscription, and touches stones. Depending on the seed, touch the stones in the shown sequence or toggle stones whose numbers add up to the target. A wrong sequence resets its progress; in the balance puzzle `*` marks lit stones. Their numbers are recorded as you find them.
- Each stone bears a number. At the gate, follow its inscription to transform the stone numbers into a code. Enter it with **1-9** (top row or numpad), then **Enter**; **Backspace** edits. Step through the gate when it opens. The creature patrols, watches ahead, follows what it sees or hears, and investigates your last known location. Running carries farther through the hedge, though several layers muffle the sound. If it catches you, you return to the entrance and keep your puzzle progress.
- Enter a lit **safehouse** to set your personal checkpoint and recover breath faster. Each doorway has a door: press **E** to open it, step inside, and press **E** again to close it. The creature can enter houses through their doorways. Closed doors buy time, but repeated strikes force them open. Loud activities attract it; repeated detections increase its hearing and search persistence. **T** toggles the ray-traced lighting pass; **M** toggles sound; **B** toggles camera motion. **Escape** pauses; **R** restarts the current maze. Choose a size from 10 to 100 and an optional decimal seed in Settings; hold Shift while clicking +/- to adjust by ten. The default for new settings is 30×30; saved dimensions are respected. A blank seed produces a new random maze; the same size and seed reproduce the layout and puzzles with the same game version and toolchain.

Safehouses have two floors: walk up the east-side stairs to a small games court.
**G** picks up or throws the basketball; **K** kicks the nearby football in the
direction you face. Hoops and goals score for everyone in co-op. Downstairs,
face the slot-cabinet handle at close range and press **E** to pull it. Reels,
handle animation and results are shared; three matching symbols win a free-play
jackpot. **F** collects keeper field notes; **J** rereads them. Captures preserve
notes; a restart resets notes, sports scores and machines. Small 10x10 mazes fit
one larger refuge; larger maps have multiple houses.

The upgraded slot cabinets, lever, bar and first-person hand/forearm meshes are
adapted from downloaded models; author, source, license and conversion records
are in [refuge asset credits](assets/models/REFUGE_SOURCES.md) and
[character credits](assets/models/README.md). Hands follow camera movement and
reach/drink interactions. The machine has mechanical reel sounds, a pull clank
and result cues; the bar has a glass clink. **E** at the bar restores breath and
reduces fear, but its noise can draw the hunter into the house. The creature can
force doors and follow players upstairs. Maze openings and closures now use
irregular seeded delays rather than a fixed interval. Co-op protocol is **15**.

A packaged build includes assets and app-local Windows runtime libraries. Extract
the entire ZIP and launch `Play.cmd`. See [the quick play guide](docs/PLAY.md).
The in-game **How to Play** opens the five-page Keeper's Field Guide. Use its
Previous/Next buttons or Left/Right arrows for survival, puzzles, refuge games
and voice-chat controls; Escape returns to the menu.
After building Release/x64, `tools/Package-Release.ps1 -Name TheHedge-Windows-x64`
creates a ZIP with dependency notices and a SHA256 file manifest.

## Four-player co-op

All players need the same game build. The host chooses the maze size and seed in Settings, then selects **Multiplayer → Host game**. The host's game listens on local TCP port **24816**. With an authenticated ngrok agent, run `ngrok tcp 24816` on the host and share the resulting `tcp://...:port` address privately. Each friend chooses **Multiplayer**, pastes that address into **Host address**, and selects **Join game**. For a test on the same computer, use `127.0.0.1:24816` without ngrok. The host accepts up to three guests, for four players total; there is no game password. Older builds are not compatible with this version.

The host controls the shared maze, creature, puzzle progress, safehouse doors, physical minigames and shifting walls. Any player can interact with the stones, doors or gate and reach the exit for a shared win. The creature can pursue and catch any player. The host's pause pauses the shared world; guests can pause locally. A host restart resets the shared run. If a guest disconnects, the others can continue and another guest can fill the open slot. If the host disconnects, guests return to the menu. Public TCP endpoints on ngrok's free plan currently require a verified payment method; see [ngrok's TCP endpoint documentation](https://ngrok.com/docs/gateway/endpoints/tcp).

For a repeatable local integration check, build `tools/CoopSmoke.vcxproj` in Release/x64 after preparing SFML, then run `build\bin\Release\CoopSmoke.exe` from the repository root. It exercises the host/join menu, four-player loopback TCP, room capacity, controls, shared puzzles, safe shifts, individual captures, rejoining and shared victory. Test port 24817 must be available.

The flashlight flickers more when the creature is close, irregular seeded lightning briefly lights the hedge, and pockets of thicker fog and darkness obscure the passages. Creature breathing, pursuit growls, wind, footsteps, shifting foliage, door creaks, whispers, and delayed thunder provide sound cues when an audio device is available; headphones reveal whether nearby creature and hedge movement is to the left or right. A fear value rises with proximity, darkness and pursuit, and drives breathing sway, lens distortion, heavier grain, a colder grade and a racing heartbeat; safehouse shelter pushes it back down. A depth-buffered OpenGL scene caches nearby hedge and foliage geometry in chunks and uses shaders for wind, flashlight and fog, with a fixed-function fallback for older drivers. The ray-traced lighting pass renders the scene to a framebuffer and ray-marches screen-space depth for reflections, contact ambient occlusion and volumetric fog; it can be disabled with **T** or falls back automatically if the driver lacks framebuffer support. Visibility is selected on the CPU. The creature uses procedural movement, while co-op survivors use baked animation from a rigged model. Asset sources are listed under `assets/models` and `assets/textures`.

## Build

Windows, Visual Studio 2026 with Desktop development with C++, the MSVC v145 x64 toolset, a Windows SDK, Git and CMake 4.2 or later are required. From the repository root, run:

```powershell
.\Prepare-Dependencies.ps1
```

The installed VS2022 v143 toolset also builds Release when passed `/p:PlatformToolset=v143` to MSBuild.

Open `NXRP.slnx`, select Debug or Release and x64, set `Maze.Graphics` as the startup project, then build and run. The script downloads and builds SFML 3.1.0. Assets are copied beside the executable at build time.

Settings persist the maze dimensions under `%LOCALAPPDATA%\NXRP\settings.txt`; seed selection lasts for the current launch. The current seed is visible in the game and on the victory screen.

## Project structure

Ground and timber PBR materials: [Powered by Poly Haven](https://polyhaven.com/),
CC0; authors and source links are in [the texture credits](assets/textures/README.md).

`Maze.Data` owns the grid and session state, `Maze.Logic` generates seeded mazes and maintains session rules, and `Maze.Graphics` contains the SFML application and first-person OpenGL view. `Maze.UnitTest` covers grid and session rules and seeded puzzle placement. The archived QA evidence predates the first-person systems.

The bar has a specific **E sit and drink** prompt; **Q** or **Space** stands up.
A downloaded stool and vessel support the sitting/sipping animation. Hands blend
source-skeleton finger curl for carrying a basketball and gripping the glass or
lever. The lever waits for reach, then the hand follows its actual knob trajectory
through pull and return. Walking is briefly locked during lever contact.

**P** at the downstairs bar plays **Keeper's Draw**, free-play five-card draw.
Press **1-5** to hold cards, then **P** to replace the rest once. Press **P** after
the result to deal again; **Q** puts the cards away. Each survivor has a separate
hand and score, synchronized by the host. Deals have directional paper scrapes
and taps and attract the hunter; the world keeps moving while you play.
There are no purchases or real-money stakes.

Nearby refuge doors now show directional first/second-strike warnings. Guests
receive the same rattles and impact events as the host, with sound positioned
for each listener. The third strike forces the door. Captures preserve the
remaining slot and poker animation timers, so a new encounter cannot lock a
machine or hand behind the previous clock. Poker cards deal and flip in sequence.

Four maze districts now connect a luggage tag, repair toolbox, cassette player
and collapsed shelter through notes and muddy boot trails. **F** reads and
collects nearby notes; **J** rereads collected pages. The damaged shelter has
walkable bypasses. Downloaded prop and footprint credits are in
[story sources](assets/models/STORY_SOURCES.md).
