/* SPDX-License-Identifier: GPL-3.0-or-later
 * The mod's rules (todano_armory_music.h) on memory laid out here: the nearest environment preset
 * within the game's 400 units, finding the class-4475 moby, and the entry watch that fires each
 * time Ratchet walks into its cuboid. */
#include <stdio.h>
#include <string.h>

#include "todano_armory_music.h"

static int failures;

#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                                \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

/* A preset row: position in game units (stored in quarters), and its track. */
static void put_row(uint8_t* rows, int i, float x, float y, float z, int16_t track) {
    uint8_t* row = rows + i * TAM_PRESET_SIZE;
    const uint16_t q[3] = {(uint16_t)(x * 4.0f), (uint16_t)(y * 4.0f), (uint16_t)(z * 4.0f)};
    memset(row, 0, TAM_PRESET_SIZE);
    memcpy(row + TAM_PRESET_X, q, sizeof q);
    memcpy(row + TAM_PRESET_TRACK, &track, 2);
}

static void nearest_preset(void) {
    uint8_t rows[3 * TAM_PRESET_SIZE];
    put_row(rows, 0, 100.0f, 100.0f, 50.0f, 3);  // outside
    put_row(rows, 1, 520.0f, 300.0f, 60.0f, 7);  // the Armory
    put_row(rows, 2, 900.0f, 900.0f, 40.0f, -1); // a row with no track

    {
        const float hero[3] = {500.0f, 310.0f, 60.0f};
        const tam_choice c = tam_nearest_preset(rows, 3, hero);
        CHECK(c.row == 1);
        CHECK(c.track == 7);
        CHECK(c.distance > 22.0f && c.distance < 23.0f); // sqrt(20^2 + 10^2) = 22.36
    }
    {
        // Farther than 400 units from every row: none, as the game.
        const float hero[3] = {2000.0f, 2000.0f, 0.0f};
        const tam_choice c = tam_nearest_preset(rows, 3, hero);
        CHECK(c.row == -1);
        CHECK(c.track == -1);
    }
    {
        // The nearest row has no track: none.
        const float hero[3] = {900.0f, 899.0f, 40.0f};
        const tam_choice c = tam_nearest_preset(rows, 3, hero);
        CHECK(c.row == 2);
        CHECK(c.track == -1);
    }
    {
        const float hero[3] = {0.0f, 0.0f, 0.0f};
        CHECK(tam_nearest_preset(rows, 0, hero).track == -1);
    }
}

static void trigger_watch(void) {
    // The moby array: three mobys, the trigger in the second.
    static uint8_t memory[0x1000];
    const uint32_t first = 0x100, end = 0x400;
    const int16_t other = 100, trigger = TAM_TRIGGER_CLASS;
    memcpy(memory + first + TAM_MOBY_CLASS, &other, 2);
    memcpy(memory + first + 0x100 + TAM_MOBY_CLASS, &trigger, 2);
    memcpy(memory + first + 0x200 + TAM_MOBY_CLASS, &other, 2);
    CHECK(tam_find_trigger(memory, first, end) == first + 0x100);
    CHECK(tam_find_trigger(memory, first, first + 0x100) == 0);

}

static void entry_watch(void) {
    {
        // Outside, then walking in: fires once, not again while he stays inside.
        int inside = -1;
        CHECK(tam_watch_entry(&inside, 0) == 0);
        CHECK(tam_watch_entry(&inside, 0) == 0);
        CHECK(tam_watch_entry(&inside, 1) == 1);
        CHECK(tam_watch_entry(&inside, 1) == 0);
        // Out and in again (a teleport or a respawn may have changed the track): fires again.
        CHECK(tam_watch_entry(&inside, 0) == 0);
        CHECK(tam_watch_entry(&inside, 1) == 1);
    }
    {
        // Put inside on the first tick (a load or respawn there): the game's preset plays, no fire.
        int inside = -1;
        CHECK(tam_watch_entry(&inside, 1) == 0);
        CHECK(tam_watch_entry(&inside, 1) == 0);
        CHECK(tam_watch_entry(&inside, 0) == 0);
        CHECK(tam_watch_entry(&inside, 1) == 1);
    }
}

static void discs(void) {
    const tam_disc* ntsc = tam_disc_for("ntsc");
    const tam_disc* pal = tam_disc_for("pal");
    CHECK(ntsc != NULL && ntsc->hero == 0x00189EA0u && ntsc->request == 0x003329B0u &&
          ntsc->in_cuboid == 0x00324168u);
    CHECK(pal != NULL && pal->hero == 0x00189F20u && pal->request == 0x00334A48u &&
          pal->in_cuboid == 0x003260A0u);
    CHECK(tam_disc_for("ntsc2") == NULL);
    CHECK(tam_disc_for("") == NULL);
}

int main(void) {
    nearest_preset();
    trigger_watch();
    entry_watch();
    discs();
    if (failures) {
        fprintf(stderr, "%d failed\n", failures);
        return 1;
    }
    puts("todano-armory-music rules: ok");
    return 0;
}
