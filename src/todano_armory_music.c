/* SPDX-License-Identifier: GPL-3.0-or-later
 * Todano's Armory music (README.md): Going Commando NTSC-U and PAL. When Ratchet walks into the Megacorp
 * Armory on Todano, class 4475 at its entrance fires, but the game asks for no music there, so the
 * outside track goes on playing. This mod asks for the Armory's track at that moment.
 *
 * Going Commando has no hook points yet (OpenRAC docs/port/MODS.md section 4), so the mod watches
 * from the tick callback: on Todano it looks for the class-4475 moby until it finds it, then asks
 * the game once a tick whether Ratchet is in that trigger's cuboid. The trigger's own state is no
 * use: it fires once for the story, and once the Armory has been played through it stays done.
 * On other levels the tick is one comparison.
 *
 * When Ratchet walks into the cuboid, the mod asks for a track through the game's own
 * request (func_L01_00323310, which crossfades): the "track" setting's, or with -1 the track of the
 * environment preset nearest Ratchet, the one the game itself plays when it puts him there. The
 * addresses are in todano_armory_music.h.
 */
#include <stdio.h>
#include <string.h>

#include "openrac/openrac_mod.h"
#include "todano_armory_music.h"

OPENRAC_MOD_DEFINE("todano-armory-music", "0.4.0")

/* ---- State (set at load, then touched only on the game thread, in tick) ---- */

static const openrac_api* s_api;
static openrac_mod* s_mod;

/* The "track" setting: -1 for the nearest preset's. */
static int s_track_setting = -1;

/* This disc's addresses (todano_armory_music.h). */
static const tam_disc* s_disc;

/* The level read last, -1 for none. */
static int s_level = -1;

/* This load of Todano's trigger: its moby's game address once found (0 before), its cuboid, and
 * whether Ratchet was in it last tick (tam_watch_entry). */
static uint32_t s_trigger;
static int32_t s_cuboid = -1;
static int s_inside = -1;

/* ---- Helpers ---- */

/* Whether [a, a + n) lies in game RAM. */
static int in_ram(uint32_t a, uint32_t n) {
    return a < TAM_RAM_SIZE && n <= TAM_RAM_SIZE - a;
}

static void log_info(const char* text) {
    s_api->log(s_mod, OPENRAC_LOG_INFO, text);
}

/* The level's moby array, [*first, *end); 0 when it is not one (between levels). */
static int moby_array(uint32_t* first, uint32_t* end) {
    const uint32_t first_at = s_api->relocate_level_data(TAM_LEVEL, TAM_MOBYS_FIRST);
    const uint32_t end_at = s_api->relocate_level_data(TAM_LEVEL, TAM_MOBYS_END);
    if (!in_ram(first_at, 4) || !in_ram(end_at, 4))
        return 0;
    *first = tam_u32(s_api->memory + first_at);
    *end = tam_u32(s_api->memory + end_at);
    return *end > *first && in_ram(*first, *end - *first) &&
           (*end - *first) / TAM_MOBY_SIZE <= TAM_MOBYS_MAX;
}

/* The track for Ratchet where he is now: the setting's, or the nearest preset's. -1 for none. */
static int choose_track(void) {
    char line[160];
    if (s_track_setting >= 0)
        return s_track_setting;

    {
        const uint32_t count_at = s_api->relocate_level_data(TAM_LEVEL, TAM_PRESET_COUNT);
        const uint32_t table_at = s_api->relocate_level_data(TAM_LEVEL, TAM_PRESET_TABLE);
        const uint32_t hero_at = s_api->relocate_data(s_disc->hero);
        int32_t count;
        uint32_t rows;
        float hero[3];
        tam_choice c;

        if (!in_ram(count_at, 4) || !in_ram(table_at, 4) || !in_ram(hero_at, 12))
            return -1;
        memcpy(&count, s_api->memory + count_at, 4);
        rows = tam_u32(s_api->memory + table_at);
        memcpy(hero, s_api->memory + hero_at, sizeof hero);
        if (count <= 0 || count > TAM_PRESETS_MAX ||
            !in_ram(rows, (uint32_t)count * TAM_PRESET_SIZE)) {
            snprintf(line, sizeof line, "no environment presets to choose from (%d)", (int)count);
            log_info(line);
            return -1;
        }
        c = tam_nearest_preset(s_api->memory + rows, count, hero);
        snprintf(
            line, sizeof line,
            "Ratchet at %.1f %.1f %.1f: nearest preset %d at %.1f units, track %d", hero[0], hero[1],
            hero[2], c.row, c.distance, c.track
        );
        log_info(line);
        return c.track;
    }
}

/* Asks the game's music for `track`, crossfading. */
static void request_track(int track) {
    typedef void (*request_fn)(int32_t track, int32_t transition);
    char line[96];
    request_fn request = (request_fn)s_api->function_at(s_disc->request);
    if (request == NULL) {
        snprintf(line, sizeof line, "Todano's track request (0x%08X) is not in this OpenRAC: no music change",
                 (unsigned)s_disc->request);
        log_info(line);
        return;
    }
    request(track, -1);
    snprintf(line, sizeof line, "the Armory's entrance: asked for track %d", track);
    log_info(line);
}

/* Whether Ratchet is in `cuboid` now, by the game's own test; -1 when it cannot be asked. */
static int hero_in_cuboid(int32_t cuboid) {
    typedef int32_t (*in_cuboid_fn)(openrac_gaddr point, int32_t cuboid);
    const uint32_t hero_at = s_api->relocate_data(s_disc->hero);
    in_cuboid_fn in_cuboid = (in_cuboid_fn)s_api->function_at(s_disc->in_cuboid);
    if (in_cuboid == NULL || !in_ram(hero_at, 16))
        return -1;
    return in_cuboid(hero_at, cuboid) != 0;
}

/* Finds this load's trigger and its cuboid. Returns 0 while it is not there yet. */
static int find_trigger(void) {
    char line[96];
    uint32_t first, end, pvars;
    if (!moby_array(&first, &end))
        return 0;
    s_trigger = tam_find_trigger(s_api->memory, first, end);
    if (s_trigger == 0)
        return 0; // the level is still placing its mobys: look again next tick
    pvars = tam_u32(s_api->memory + s_trigger + TAM_MOBY_PVARS);
    s_cuboid = in_ram(pvars, 4) ? (int32_t)tam_u32(s_api->memory + pvars) : -1;
    snprintf(
        line, sizeof line, "the Armory's entrance: trigger at 0x%08X, cuboid %d, state %u",
        (unsigned)s_trigger, (int)s_cuboid, (unsigned)s_api->memory[s_trigger + 0x20]
    );
    log_info(line);
    return 1;
}

static void tick(void* user) {
    (void)user;
    const int level = s_api->loaded_level();
    int now;
    if (level != s_level) {
        // Another level, or Todano loaded again: its trigger starts over.
        s_level = level;
        s_trigger = 0;
        s_cuboid = -1;
        s_inside = -1;
    }
    if (level != TAM_LEVEL)
        return;

    if (s_trigger == 0 && !find_trigger())
        return;

    // The trigger's moby may go (the level unloading under us): look for it again if it has.
    if (!in_ram(s_trigger, TAM_MOBY_SIZE) ||
        tam_s16(s_api->memory + s_trigger + TAM_MOBY_CLASS) != TAM_TRIGGER_CLASS) {
        s_trigger = 0;
        s_inside = -1;
        return;
    }
    if (s_cuboid < 0)
        return; // a trigger with no cuboid: nothing to watch

    // Not the trigger's state: it fires once for the story and stays done in every later load.
    // Ratchet walking into its cuboid is what the music follows, each time he walks in. Asking for
    // the track already playing changes nothing (func_L01_00323310 returns early).
    now = hero_in_cuboid(s_cuboid);
    if (now < 0)
        return;
    if (tam_watch_entry(&s_inside, now)) {
        const int track = choose_track();
        if (track >= 0)
            request_track(track);
        else
            log_info("the Armory's entrance: no track to ask for (set \"track\")");
    }
}

/* Called by the host when it loads the library: checks the game and the disc, reads the setting and
 * registers the tick callback. Returns 0 on success; on failure it tells the host why. */
OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* api, openrac_mod* mod) {
    s_api = api;
    s_mod = mod;
    s_level = -1;
    s_trigger = 0;
    s_cuboid = -1;
    s_inside = -1;

    // The addresses are Going Commando's level program for Todano, USA v1.01 or Europe v1.00.
    if (strcmp(api->game, "rac2") != 0) {
        api->fail(mod, "todano-armory-music is for Ratchet & Clank: Going Commando (rac2)");
        return 1;
    }
    s_disc = tam_disc_for(api->version);
    if (s_disc == NULL) {
        char reason[112];
        snprintf(reason, sizeof reason,
                 "todano-armory-music knows the NTSC-U v1.01 and PAL discs, not %s", api->version);
        api->fail(mod, reason);
        return 1;
    }

    {
        const int64_t track = api->setting_int(mod, "track");
        s_track_setting = track >= 0 && track <= 255 ? (int)track : -1;
    }

    if (api->on_frame(mod, tick, NULL) != 0) {
        api->fail(mod, "no tick callback in this OpenRAC");
        return 1;
    }
    return 0;
}
