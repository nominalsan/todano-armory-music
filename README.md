# Todano Armory music

On Todano, walking into the Megacorp Armory leaves the outside music playing. This mod switches
to the Armory's track as Ratchet walks in, with the crossfade the game uses for its own track
changes.

Going Commando, NTSC-U (USA v1.01) only. Native mod, OpenRAC mod interface 1.0 or newer.

## Why the track does not change

rac2-decomp ([OpenRAC/rac2-decomp](https://github.com/OpenRAC/rac2-decomp)) shows how Todano
changes its music. Every in-level change goes through one request, `func_L01_00323310(track,
transition)`, and only the Teleporter, the Infiltrator Lock and the Electrolyzer puzzle call it.
Each carries its track in its own placed data. The other starter, the environment presets
(`func_L00_00311DC0`), runs only when the game puts Ratchet somewhere (the level's start, a
respawn, a teleport), and starts its track only when no track plays.

The object at the Armory's entrance is class 4475 (`func_L12_004254A8`). When Ratchet first
stands in its cuboid it sets a story flag (`0x001395DB`) and switches itself off. It asks for no
music, and nothing in the game reads that flag back by its address, so walking in changes nothing.

## What the mod does

On Todano the mod looks for the class-4475 trigger until it finds it, then asks the game once a
tick (IsPointInCuboid) whether Ratchet is in the trigger's cuboid. When he walks in, the mod asks
the game's music for a track through the same request the Teleporter uses:

- **Track** `-1` (the default): the track of the environment preset nearest Ratchet at that
  moment, the one the game would play had it put him there (nearest within 400 units, as the
  game).
- **Track** `0` to `255`: that track.

The log says which preset and track it chose. If the default picks the outside track (the
nearest preset can be the one outside the door), set **Track** to the Armory's number from that
log or from a level editor.

The mod does not follow the trigger's own state: that is a one-shot for the story, and once the
Armory has been played through the trigger is already done whenever Todano loads, so a mod waiting
for it to fire would never change the music again. The cuboid is there every time.

The mod changes nothing else. It asks again each time Ratchet walks in (asking for the track that
already plays changes nothing), but not when the game puts him inside (a load or a respawn there:
the game's preset plays then). The track stays when Ratchet walks back out, until something else
changes it (a teleporter, a puzzle, a respawn). Going Commando has no hook points yet
([MODS.md](https://github.com/OpenRAC/OpenRAC/blob/master/docs/port/MODS.md), section 4), so the
mod watches the cuboid from the tick callback: one call into the game's cuboid test a tick on
Todano, a single comparison on other levels. It only reads game memory
and calls the game's own request function.

## Settings

| Key | Type | Default | What |
|---|---|---|---|
| `track` | int | `-1` | the Armory's track, `-1` for the nearest preset's |

## Install

Download the zip from the [releases](https://github.com/nominalsan/todano-armory-music/releases)
and add it as a mod in OpenRAC.

## Build

```sh
python3 tools/fetch_sdk.py        # the OpenRAC mod SDK that sdk.lock pins, into .sdk/
cmake -S . -B build -G Ninja
cmake --build build && ctest --test-dir build
python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/todano_armory_music -o dist
```

## Tests

`ctest` runs the rules on memory laid out by the test: the nearest preset
within range, finding the trigger, and the entry watch firing each time Ratchet walks in.
