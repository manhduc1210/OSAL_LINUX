// tests/fake_gpiod_min.c
// This file overrides only the gpiod symbols that our HAL actually calls.
// Build this file into unit-test target instead of linking real -lgpiod.

#include <string.h>
#include <stdlib.h>
#include <time.h>

/* --------- fake internal model ---------- */

typedef struct {
    int requested;
    int dir;     // 0 = in, 1 = out
    int value;
    int want_event;
    int pending_event;
    int pending_event_type;
    struct timespec pending_ts;
} fake_line_t;

typedef struct {
    char name[32];
    int  num_lines;
    fake_line_t lines[16];
} fake_chip_t;

static fake_chip_t s_chip = {0};

/* helper to init once */
static void fake_init_once(void)
{
    if (s_chip.num_lines != 0) return;
    memset(&s_chip, 0, sizeof(s_chip));
    strcpy(s_chip.name, "gpiochip0");
    s_chip.num_lines = 8;
}

/* ========= symbols that HAL expects from libgpiod ========= */

/* open/close chip */
struct gpiod_chip { int dummy; };
struct gpiod_line { int dummy; };

struct gpiod_chip* gpiod_chip_open_by_name(const char *name)
{
    fake_init_once();
    if (!name) return NULL;
    if (strcmp(name, s_chip.name) != 0) return NULL;
    /* We return a fake pointer; we can just return (struct gpiod_chip*)&s_chip */
    return (struct gpiod_chip*)&s_chip;
}

void gpiod_chip_close(struct gpiod_chip *chip)
{
    (void)chip; // nothing, we keep static chip
}

int gpiod_chip_num_lines(struct gpiod_chip *chip)
{
    (void)chip;
    fake_init_once();
    return s_chip.num_lines;
}

/* get line by offset */
struct gpiod_line* gpiod_chip_get_line(struct gpiod_chip *chip, int offset)
{
    (void)chip;
    fake_init_once();
    if (offset < 0 || offset >= s_chip.num_lines) return NULL;
    /* We return pointer to the line struct casted to gpiod_line */
    return (struct gpiod_line*)&s_chip.lines[offset];
}

/* optional: some HALs call gpiod_line_name() */
const char* gpiod_line_name(struct gpiod_line *line)
{
    fake_line_t *l = (fake_line_t*)line;
    /* we don't store names, can return NULL */
    (void)l;
    return NULL;
}

void gpiod_line_release(struct gpiod_line *line)
{
    fake_line_t *l = (fake_line_t*)line;
    if (!l) return;
    l->requested = 0;
}

/* request I/O */
int gpiod_line_request_output(struct gpiod_line *line,
                              const char *consumer,
                              int default_val)
{
    (void)consumer;
    fake_line_t *l = (fake_line_t*)line;
    if (!l) return -1;
    l->requested = 1;
    l->dir = 1;
    l->value = default_val ? 1 : 0;
    return 0;
}

int gpiod_line_request_input(struct gpiod_line *line, const char *consumer)
{
    (void)consumer;
    fake_line_t *l = (fake_line_t*)line;
    if (!l) return -1;
    l->requested = 1;
    l->dir = 0;
    return 0;
}

/* some HALs may request events */
#define GPIOD_LINE_EVENT_RISING_EDGE  1
#define GPIOD_LINE_EVENT_FALLING_EDGE 2

int gpiod_line_request_rising_edge_events(struct gpiod_line *line,
                                          const char *consumer)
{
    (void)consumer;
    fake_line_t *l = (fake_line_t*)line;
    if (!l) return -1;
    l->requested = 1;
    l->dir = 0;
    l->want_event = 1;
    return 0;
}

int gpiod_line_request_falling_edge_events(struct gpiod_line *line,
                                           const char *consumer)
{
    (void)consumer;
    fake_line_t *l = (fake_line_t*)line;
    if (!l) return -1;
    l->requested = 1;
    l->dir = 0;
    l->want_event = 1;
    return 0;
}

int gpiod_line_request_both_edges_events(struct gpiod_line *line,
                                         const char *consumer)
{
    (void)consumer;
    fake_line_t *l = (fake_line_t*)line;
    if (!l) return -1;
    l->requested = 1;
    l->dir = 0;
    l->want_event = 1;
    return 0;
}

/* I/O */
int gpiod_line_set_value(struct gpiod_line *line, int value)
{
    fake_line_t *l = (fake_line_t*)line;
    if (!l || !l->requested || l->dir != 1) return -1;
    l->value = value ? 1 : 0;
    return 0;
}

int gpiod_line_get_value(struct gpiod_line *line)
{
    fake_line_t *l = (fake_line_t*)line;
    if (!l || !l->requested) return -1;
    return l->value;
}

/* events */
struct gpiod_line_event {
    struct timespec ts;
    int event_type;
};

int gpiod_line_event_wait(struct gpiod_line *line, const struct timespec *timeout)
{
    (void)timeout;
    fake_line_t *l = (fake_line_t*)line;
    if (!l || !l->requested) return -1;
    if (l->pending_event) return 1; // ready
    return 0; // timeout
}

int gpiod_line_event_read(struct gpiod_line *line, struct gpiod_line_event *ev)
{
    fake_line_t *l = (fake_line_t*)line;
    if (!l || !l->pending_event) return -1;
    if (ev) {
        ev->ts = l->pending_ts;
        ev->event_type = l->pending_event_type;
    }
    l->pending_event = 0;
    return 0;
}

/* ---------- helper for tests (optional) ----------
   You can declare this in your test file as extern
   to inject events into a line.
*/
void fake_gpiod_inject_event(int line_offset, int event_type)
{
    fake_init_once();
    if (line_offset < 0 || line_offset >= s_chip.num_lines) return;
    fake_line_t *l = &s_chip.lines[line_offset];
    l->pending_event = 1;
    l->pending_event_type = event_type;
    clock_gettime(CLOCK_REALTIME, &l->pending_ts);
}
