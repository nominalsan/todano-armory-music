/* SPDX-License-Identifier: GPL-3.0-or-later
 * Todano's Armory music (README.md): the rules the mod applies, apart from the host, so the tests
 * can run them on memory of their own.
 *
 * Going Commando NTSC-U (USA v1.01), level 12 (Todano). The addresses and layouts below come from
 * rac2-decomp (github.com/OpenRAC/rac2-decomp), measured against the retail level program:
 *
 * - Class 4475's update (func_L12_004254A8, src/levels/12_todano/L12_0041E3E8.c): while its state
 *   byte (+0x20) is 0 and the hero's position (D_00189EA0) is in the cuboid its first pvar names
 *   (IsPointInCuboid), it sets the story flag 0x001395DB and switches itself to state 1. It never
 *   asks for music: walking into the Armory leaves the outside track playing. Its state is a
 *   one-shot for the story: once the Armory has been played through it is no longer 0 when the
 *   level loads, so the mod watches the cuboid itself, not the state.
 * - The level's mobys: from *D_L12_001B2E9C up to *D_L12_001B2EA0, 0x100 bytes each, the class a
 *   short at +0xAA (f_002FFEC0, the game's own "nearest moby of a class" search).
 * - The environment presets (func_L00_00311DC0, src/levels/shared/L00_002FC7A0-00311DC0-g8.c):
 *   D_L12_001B2F10 rows at *D_L12_001B2F40, 32 bytes each, the position u16 x, y, z at +4 in
 *   quarter units and the music track a short at +0xC. On a warp the game takes the nearest row
 *   within 400 units and starts its track, but only when no track plays.
 * - The track request (func_L01_00323310 at 0x003329B0 in level 12,
 *   src/levels/shared/L01_00323310-pre.c): void(int track, int transition), -1 for no transition:
 *   the music system crossfades to the track.
 */
#ifndef TODANO_ARMORY_MUSIC_H
#define TODANO_ARMORY_MUSIC_H

#include <stdint.h>
#include <string.h>

/* The game's number for Todano (loaded_level()). */
#define TAM_LEVEL 12

/* The class whose update fires at the Armory's entrance. */
#define TAM_TRIGGER_CLASS 4475

/* Todano's globals (relocate_level_data(12, ...)). */
#define TAM_MOBYS_FIRST 0x001B2E9Cu
#define TAM_MOBYS_END 0x001B2EA0u
#define TAM_PRESET_COUNT 0x001B2F10u
#define TAM_PRESET_TABLE 0x001B2F40u

/* The executable's hero position, x y z floats (relocate_data). */
#define TAM_HERO_POSITION 0x00189EA0u

/* func_L01_00323310 in Todano's program (function_at). */
#define TAM_REQUEST_TRACK 0x003329B0u

/* IsPointInCuboid (func_002ACBC8) in Todano's program (function_at): int(point, cuboid), nonzero
 * when the point is in the cuboid. */
#define TAM_POINT_IN_CUBOID 0x00324168u

/* A moby's layout: its pvars at +0x68, whose first word is class 4475's cuboid (-1 for none). */
#define TAM_MOBY_SIZE 0x100u
#define TAM_MOBY_CLASS 0xAAu
#define TAM_MOBY_PVARS 0x68u

/* A preset's layout. */
#define TAM_PRESET_SIZE 32u
#define TAM_PRESET_X 4u
#define TAM_PRESET_TRACK 0xCu

/* The game's reach for a preset, in its units. */
#define TAM_PRESET_RANGE 400.0f

/* The most mobys and presets a level has that the mod will walk; more means the memory is not what
 * it expects. */
#define TAM_MOBYS_MAX 4096u
#define TAM_PRESETS_MAX 1024

/* Game memory below this is RAM (32 MB). */
#define TAM_RAM_SIZE 0x02000000u

/* A preset row chosen for the hero, or none (track -1). */
typedef struct tam_choice {
    int track;      /* the row's music track, -1 for none in range */
    int row;        /* the row, -1 for none */
    float distance; /* from the hero to the row */
} tam_choice;

/* The little-endian u16 / s16 / u32 at p. */
static inline uint16_t tam_u16(const uint8_t* p) {
    uint16_t v;
    memcpy(&v, p, 2);
    return v;
}

static inline int16_t tam_s16(const uint8_t* p) {
    int16_t v;
    memcpy(&v, p, 2);
    return v;
}

static inline uint32_t tam_u32(const uint8_t* p) {
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}

/* The preset nearest to `hero` (x, y, z) among the `count` 32-byte rows at `rows`, as
 * func_L00_00311DC0 chooses one: the nearest, and none when it is farther than 400 units. A row's
 * track below 0 counts as none. Distances are compared squared; the game compares lengths, which
 * orders the rows the same way. */
static inline tam_choice tam_nearest_preset(const uint8_t* rows, int count, const float hero[3]) {
    tam_choice best = {-1, -1, 0.0f};
    float best_d2 = 0.0f;
    for (int i = 0; i < count; ++i) {
        const uint8_t* row = rows + (size_t)i * TAM_PRESET_SIZE;
        const float dx = (float)tam_u16(row + TAM_PRESET_X) * 0.25f - hero[0];
        const float dy = (float)tam_u16(row + TAM_PRESET_X + 2) * 0.25f - hero[1];
        const float dz = (float)tam_u16(row + TAM_PRESET_X + 4) * 0.25f - hero[2];
        const float d2 = dx * dx + dy * dy + dz * dz;
        if (best.row < 0 || d2 < best_d2) {
            best.row = i;
            best_d2 = d2;
        }
    }
    if (best.row < 0 || best_d2 > TAM_PRESET_RANGE * TAM_PRESET_RANGE) {
        best.row = -1;
        return best;
    }
    {
        const int track = tam_s16(rows + (size_t)best.row * TAM_PRESET_SIZE + TAM_PRESET_TRACK);
        float d = best_d2;
        /* A square root without libm: a few Newton steps from d are enough for a log line. */
        float r = d > 1.0f ? d : 1.0f;
        for (int k = 0; k < 24; ++k)
            r = 0.5f * (r + d / r);
        best.distance = d > 0.0f ? r : 0.0f;
        best.track = track >= 0 ? track : -1;
    }
    return best;
}

/* The game address of the first class-4475 moby in the array [first, end) of `memory`, 0 for none.
 * The array must lie in `memory`. */
static inline uint32_t tam_find_trigger(const uint8_t* memory, uint32_t first, uint32_t end) {
    for (uint32_t m = first; m + TAM_MOBY_SIZE <= end; m += TAM_MOBY_SIZE) {
        if (tam_s16(memory + m + TAM_MOBY_CLASS) == TAM_TRIGGER_CLASS)
            return m;
    }
    return 0;
}

/* The entrance's watch: fed whether Ratchet is in the trigger's cuboid each tick, it fires when he
 * goes from outside to inside. `*inside` is the watch's own: -1 until the first tick (Ratchet put
 * inside, by a respawn or a load, does not count as walking in: the game starts the preset's track
 * then), else whether he was inside last tick. Returns 1 the tick he walks in. */
static inline int tam_watch_entry(int* inside, int now) {
    const int was = *inside;
    *inside = now != 0;
    return was == 0 && now != 0;
}

#endif
