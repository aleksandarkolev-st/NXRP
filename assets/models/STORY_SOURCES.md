# Story prop sources

All story props derive from downloaded meshes. Conversion is in
`tools/prepare_story_props.py`; it applies source node transforms, scales and
recentres meshes, and bakes material atlases.

| Runtime files | Original asset | Author | License |
| --- | --- | --- | --- |
| story-luggage.nxm / .png | [Vintage Suitcase](https://polyhaven.com/a/vintage_suitcase) | Maximilian Schuster | CC0 1.0 |
| story-toolbox.nxm / .png | [Metal Toolbox](https://polyhaven.com/a/metal_toolbox) | Mateusz Sadek | CC0 1.0 |
| story-player.nxm / .png | [Portable Cassette Player](https://polyhaven.com/a/portable_cassette_player) | Mateusz Sadek | CC0 1.0 |
| story-ruin.nxm / .png, story-sign.nxm / .png | [Construction site cabin](https://www.innerscene.com/tools/library/3d-parts/construction-site-cabin-6da41273) | Innerscene | CC0 1.0 |

Poly Haven sources use their 1K glTF downloads. Author records are available at
`https://api.polyhaven.com/info/<asset_id>` using the IDs in the links above.
The cabin is deformed to suggest roof collapse; the sign retains a subset of
the source door triangles. Texture colors are adjusted for weathering.

`assets/textures/story-foot-left.png` and `story-foot-right.png` are resized
copies of [Footprints by Paulo Moisés](https://opengameart.org/content/footprints),
CC0 1.0. Runtime tints and rotates the decals along reachable passage routes.

The writing and district names are original game content. Source meshes are
redistributed under their CC0 dedications; see
[CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).
