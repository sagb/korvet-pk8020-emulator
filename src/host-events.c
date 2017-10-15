/*
 *  (c) 2017 Yauheni Kaliuta <y.kaliuta@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 */

/*
 * This file contatin the implementation of the host events
 * related abstractions. Examples: input events (keyboard,
 * mouse), timer.
 *
 * For now I'm trying to keep all events related code in this one
 * file because if we switch to the event based library, it
 * becomes easier to forward the events.
 */

/* for asprintf */
#define _GNU_SOURCE

#include "host-events.h"
#include "host.h"
#include "queue.h"
#include "verbose.h"

#include <allegro.h>
#include <string.h>

#define MAX_EVENTS 512

/* Globals */
static struct queue *event_queue;
static bitmap_t current_mods;

void host_event_wait(struct host_event *ev)
{
    queue_wait(event_queue);
    queue_pop(event_queue, ev);
}

bool host_event_pop(struct host_event *ev)
{
    ev = queue_pop(event_queue, ev);
    return ev != NULL;
}

void host_event_push(struct host_event *ev)
{
    while (queue_is_full(event_queue))
        pr_once("Event queue got stuck\n");
    queue_push(event_queue, ev);
}

void host_event_init_common(struct host_event *ev,
				   enum event_type type)
{
    /* May add timestamp later */
    memset(ev, 0, sizeof(*ev));
    ev->type = type;
}

char *host_event_to_str(struct host_event *ev)
{
    char *str;
    const char *keyname;

    switch (ev->type) {
    case HOST_KEY_UP:
    case HOST_KEY_DOWN:
        keyname = scancode_to_name(ev->key.code);
        asprintf(&str, "%d, mods 0x%08x, key %s",
                 ev->type, ev->key.mods, keyname);
        break;
    default:
        asprintf(&str, "%d", ev->type);
    }

    return str;
}

static void host_event_kbd_init(struct host_event *ev,
				enum event_type type,
				bitmap_t mods,
				int scancode)
{
    host_event_init_common(ev, type);

    ev->key.mods = mods;
    ev->key.code = scancode;
}

static enum key_mods host_scancode_to_mod(int scancode)
{
    switch(scancode) {
    case KEY_ALT:
    case KEY_ALTGR:
        return HOST_MOD_ALT;
    case KEY_LSHIFT:
    case KEY_RSHIFT:
        return HOST_MOD_SHIFT;
    case KEY_LCONTROL:
    case KEY_RCONTROL:
        return HOST_MOD_CTRL;
    }
    return 0;
}

static void key_callback(int event)
{
    bool is_down = !BIT_TEST(event, 7);
    int scancode = BIT_CLEAR(event, 7);
    bool is_mod;
    enum key_mods mod;
    struct host_event ev;
    enum event_type ev_type;

    mod = host_scancode_to_mod(scancode);
    is_mod = (mod != HOST_MOD_NONE);

    if (is_mod) {
        if (is_down)
            BIT_SET(current_mods, mod);
        else
            BIT_CLEAR(current_mods, mod);
        return;
    }

    ev_type = is_down ? HOST_KEY_DOWN : HOST_KEY_UP;

    host_event_kbd_init(&ev, ev_type, current_mods, scancode);
    host_event_push(&ev);
}

int host_events_init(void)
{
    /* global */
    event_queue = queue_new(MAX_EVENTS, sizeof(struct host_event));
    if (event_queue == NULL)
        return -1;

    keyboard_lowlevel_callback = key_callback;

    return 0;
}

void host_events_shutdown(void)
{
    keyboard_lowlevel_callback = NULL;
    queue_destroy(event_queue);
    event_queue = NULL;
}
