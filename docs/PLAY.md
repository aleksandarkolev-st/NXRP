# The Hedge

Launch `Play.cmd` or `Maze.Graphics.exe`. Windows x64 and an OpenGL-capable
graphics driver are required. Extract the entire archive before playing.

How to Play in the main menu opens Keeper's Field Guide. Its five readable pages
cover survival, the gate, refuges, games and co-op. Use Previous/Next or the
Left/Right arrow keys. Back or Escape returns to the main menu.

WASD moves, mouse looks, Shift runs, Space jumps, Ctrl/C crouches, E interacts, Escape pauses. Follow the
entrance inscription, find the three engraved stones, then decipher the gate.
Running draws the hunter. Closed refuge doors block you: E opens or closes them.

Houses have colourful two-floor interiors and act as checkpoints. They do not
protect you from the hunter: it can force doors and climb the stairs. Walk up
the east-side stairs to the games court. G picks up or throws a nearby basketball;
face the hoop before throwing. K kicks the football in the direction you face.
Throwing includes a short wind-up before release; keep facing your target.
Goals and baskets reset the ball and update shared scores. Balls stay upstairs.

Downstairs, face the slot-cabinet handle at close range and press E to pull. The monster can hear the reels.
Three matching reel symbols are a jackpot; play is free. E at the bar seats you and lifts a glass to drink, restoring breath. E drinks again after the cooldown; Q or Space stands up. Clinking glass attracts the hunter. Closed doors only delay it. F collects keeper notes
inside refuges; J cycles collected notes. Puzzle clues and notes survive captures.
Restart clears the run, sports scores, slots and notes.

T toggles screen-space ray-traced lighting, B toggles camera motion, M mutes.
Lighting traces the visible depth buffer for reflections and contact shading;
offscreen geometry cannot contribute to those reflections. A compatible driver
is needed, with an automatic fallback if framebuffer support is unavailable.
Ground and timber use sourced normal and roughness maps. Wet mud catches light
and screen-space reflections; refuge timber stays dry. Contact shadows use
several nearby light samples for softer edges. T controls these screen-space
effects; material detail remains active when they are off.

For co-op, all players need this same build. Multiplayer > Host listens on TCP
24816. Guests join the host address. On one computer use 127.0.0.1:24816; across
the internet use a configured TCP tunnel or port forwarding. The host controls
shared doors, puzzles, balls, slots and the hunter. Host pause pauses the world.

Hold V for proximity voice chat while playing co-op. Release V to stop microphone
capture. Speech fades over 12 maze units, is muffled by walls/doors and attracts
the hunter. F8 turns voice off/on; F1-F4 mute individual players locally (host is
player 1). The HUD shows transmitting, active speakers and device errors. Voice
is not recorded. Pausing or losing window focus stops capture and clears audio.
M controls game sounds separately from voice. A working Windows microphone and
output device are required; use F8 twice to retry after changing a device.

The hunter investigates sounds and its last sighting. It pauses at a lost trail,
checks nearby routes, then gives up without further evidence. Repeatedly taking
an escape it can see makes that route more likely to be checked. Quiet crouching
and changing routes help. Upper floors and closed doors block sight; running,
speech, slots and glasses can still give you away. Door strikes warn before a
forced entry. It cannot track a silent hidden player through the floor.

Asset attribution is included in assets/models/README.md,
assets/textures/README.md and assets/fonts/README.md.

The hands close around carried basketballs, open with throw follow-through, and
reach to the model's actual lever knob during its pull. Hold interactions briefly
lock walking; the bar has a visible stool and a specific sit/drink prompt.
All co-op players need this protocol-15 build for matching world and voice messages.

Door warnings are shared in co-op: nearby doors visibly rattle and their impacts
come from the door's position relative to each listener. A warning points left,
right, ahead or behind. The first strike buys time; leave on the second, because
the third forces the door. Distant door sounds fade out. Opening a damaged door
does not repair it. Poker cards deal and turn over in sequence; held cards stay
face-up during the replacement draw.

At the downstairs bar, P opens Keeper's Draw and deals five cards. Press 1-5
(or numpad 1-5) to toggle holds, then P to draw replacement cards once. Your
result awards free-play points; P starts the next hand and Q hides the cards.
Hands and scores belong to individual survivors and reset with the run. Each
deal/draw needs a moment before another action. The game keeps running, and
card noise can attract the hunter. There are no real-money stakes.

Look for luggage, a toolbox, a recorder and a collapsed shelter in the four
named districts. F collects their linked notes; J cycles collected pages.
Boot trails follow actual passages, and the damaged shelter has bypasses.
