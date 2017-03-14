#include <config.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/mman.h>
#include <xkbcommon/xkbcommon.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>

#define _cleanup_(_x) __attribute__((cleanup(_x)))
/*
 * DEFINE_TRIVIAL_CLEANUP_FUNC() - define helper suitable for _cleanup_()
 * @_type: type of object to cleanup
 * @_func: function to call on cleanup
 */
#define DEFINE_TRIVIAL_CLEANUP_FUNC(_type, _func)               \
        static inline void _func##p(_type *_p)                  \
        {                                                       \
                if (*_p)                                        \
                        _func(*_p);                             \
        }                                                       \
        struct __useless_struct_to_allow_trailing_semicolon__

DEFINE_TRIVIAL_CLEANUP_FUNC(struct xkb_context *, xkb_context_unref);
static inline void freep(void *p) { free(*(void**)p); }

#define PATH_PREFIX "/org/freedesktop/WooDoTool1"
#define NAME_PREFIX "org.freedesktop.wooDoTool1"

struct wdt_keyboard {
    char *path;
    char *name;

    struct xkb_context *xkb;
    struct xkb_keymap *keymap;
};

struct wdt {
    sd_event *event;
    sd_bus *bus;

    struct wdt_keyboard *keyboards[64];
    int nkeyboards;
};

static int
wdt_kbd_get_name(sd_bus *bus,
                 const char *path,
                 const char *interface,
                 const char *property,
                 sd_bus_message *reply,
                 void *userdata,
                 sd_bus_error *error)
{
    struct wdt_keyboard *kbd = userdata;

    return sd_bus_message_append(reply, "s", kbd->name);
}

static int
wdt_kbd_set_xkbmap(sd_bus_message *m,
                   void *userdata,
                   sd_bus_error *error)
{
    struct wdt_keyboard *kbd = userdata;
    _cleanup_(xkb_context_unrefp) struct xkb_context *xkb = NULL;
    struct xkb_keymap *keymap;
    char *map_str;
    int fd;
    int r;
    size_t size;

    r = sd_bus_message_read(m, "h", &fd);
    if (r < 0)
        return r;

    r = sd_bus_message_read(m, "u", &size);
    if (r < 0)
        return r;

    map_str = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (map_str == MAP_FAILED) {
        return -errno;
    }

    xkb = xkb_context_new(0);
    keymap = xkb_keymap_new_from_string(xkb,
                                        map_str,
                                        XKB_KEYMAP_FORMAT_TEXT_V1,
                                        0);
    munmap(map_str, size);
    close(fd);

    if (!keymap) {
        fprintf(stderr, "failed to compile keymap\n");
        return -errno;
    }

    kbd->xkb = xkb;
    kbd->keymap = keymap;
    xkb = NULL;

    return sd_bus_reply_method_return(m, "");
}

static int
wdt_kbd_press(sd_bus_message *m,
              void *userdata,
              sd_bus_error *error)
{
    unsigned int keycode;
    int r;

    r = sd_bus_message_read(m, "u", &keycode);
    if (r < 0)
        return r;

    printf("key press: %d\n", keycode);

    return sd_bus_reply_method_return(m, "");
}

static int
wdt_kbd_release(sd_bus_message *m,
                void *userdata,
                sd_bus_error *error)
{
    unsigned int keycode;
    int r;

    r = sd_bus_message_read(m, "u", &keycode);
    if (r < 0)
        return r;

    printf("key release: %d\n", keycode);

    return sd_bus_reply_method_return(m, "");
}

const sd_bus_vtable wdt_kbd_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Name", "s", wdt_kbd_get_name, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_METHOD("SetXKBMap", "hu", "", wdt_kbd_set_xkbmap, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Press", "u", "", wdt_kbd_press, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Release", "u", "", wdt_kbd_release, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END,
};

static int
wdt_mgr_get_keyboard(sd_bus_message *m,
                     void *userdata,
                     sd_bus_error *error)
{
    struct wdt *ctx = userdata;
    struct wdt_keyboard *kbd;
    char path[5];
    int r;
    pid_t pid = 0;
    sd_bus_creds *creds;
    const char *name,
               *comm = "unknown";

    /* Can't get the pid directly from the message creds, we have to go
     * through the name */
    creds = sd_bus_message_get_creds(m); /* do _not_ unref */
    if ((sd_bus_creds_get_mask(creds) & SD_BUS_CREDS_UNIQUE_NAME) == 0)
        return -ENXIO;

    sd_bus_creds_get_unique_name(creds, &name); /* _do_ unref */
    r = sd_bus_get_name_creds(ctx->bus, name,
                              SD_BUS_CREDS_PID|SD_BUS_CREDS_COMM|SD_BUS_CREDS_AUGMENT,
                              &creds);
    if (r < 0)
        return r;

    if ((sd_bus_creds_get_mask(creds) & SD_BUS_CREDS_PID) != 0) {
        r = sd_bus_creds_get_pid(creds, &pid);
        if (r < 0)
            return r;
    } else {
        printf("ERROR: PID not available\n");
    }

    if ((sd_bus_creds_get_mask(creds) & SD_BUS_CREDS_COMM) != 0) {
        r = sd_bus_creds_get_comm(creds, &comm);
        if (r < 0)
            return r;
    } else {
        printf("ERROR: COMM not available\n");
    }

    r = sd_bus_message_read(m, "");
    if (r < 0)
        return r;

    kbd = calloc(1, sizeof(*kbd));
    sprintf(path, "%d", ctx->nkeyboards);
    ctx->keyboards[ctx->nkeyboards] = kbd;
    ctx->nkeyboards++;

    asprintf(&kbd->name, "kbd-pid-%d-%s", pid, comm);

    r = sd_bus_path_encode(PATH_PREFIX "/keyboards",
                           path,
                           &kbd->path);

    printf("New keyboard '%s' available at: %s\n", kbd->name, kbd->path);

    sd_bus_creds_unref(creds);

    return sd_bus_reply_method_return(m, "o", kbd->path);
}

static int
wdt_find_kbd(sd_bus *bus,
             const char *path,
             const char *interface,
             void *userdata,
             void **found,
             sd_bus_error *error)
{
    struct wdt *ctx = userdata;
    _cleanup_(freep) char *name = NULL;
    int r;

    r = sd_bus_path_decode_many(path,
                                PATH_PREFIX "/keyboards/%",
                                &name);
    if (r <= 0)
        return r;

    /* FIXME: return the correct keyboard */
    *found = ctx->keyboards[0];

    return *found ? 1 : 0;
}

static int
wdt_list_keyboards(sd_bus *bus,
                   const char *path,
                   void *userdata,
                   char ***paths_out,
                   sd_bus_error *error)
{
    struct wdt *ctx = userdata;
    char **paths = calloc(ctx->nkeyboards + 1, sizeof(char *));
    int i;

    for (i = 0; i < ctx->nkeyboards; i++)
        paths[i] = strdup(ctx->keyboards[i]->path);

    paths[i] = NULL;
    *paths_out = paths;

    return 1;
}

static const sd_bus_vtable wdt_mgr_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("GetKeyboard", "", "o", wdt_mgr_get_keyboard, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END,
};

static int
init(struct wdt *ctx)
{
    int r;

    r = sd_event_default(&ctx->event);
    if (r < 0)
        return r;

    r = sd_event_set_watchdog(ctx->event, true);
    if (r < 0)
        return r;

    r = sd_bus_open_user(&ctx->bus);
    if (r < 0)
        return r;

    r = sd_bus_add_object_vtable(ctx->bus,
                                 NULL,
                                 PATH_PREFIX,
                                 NAME_PREFIX ".Manager",
                                 wdt_mgr_vtable,
                                 ctx);
    if (r < 0)
        return r;

    r = sd_bus_add_fallback_vtable(ctx->bus,
                                   NULL,
                                   PATH_PREFIX "/keyboards",
                                   NAME_PREFIX ".Keyboard",
                                   wdt_kbd_vtable,
                                   wdt_find_kbd,
                                   ctx);
    if (r < 0)
        return r;

    r = sd_bus_add_node_enumerator(ctx->bus,
                                   NULL,
                                   PATH_PREFIX "/keyboards",
                                   wdt_list_keyboards,
                                   ctx);
    if (r < 0)
        return r;

    r = sd_bus_request_name(ctx->bus, NAME_PREFIX,
                            SD_BUS_NAME_ALLOW_REPLACEMENT|SD_BUS_NAME_REPLACE_EXISTING);
    if (r < 0)
        return r;

    r = sd_bus_attach_event(ctx->bus, ctx->event, 0);
    if (r < 0)
        return r;

    return 0;
}

static int
run(struct wdt *ctx)
{
    return sd_event_loop(ctx->event);
}

int
main(int argc, char **argv)
{
    struct wdt ctx = {0};
    int r;

    r = init(&ctx);
    if (r < 0)
        goto fail;

    run(&ctx);

    return EXIT_SUCCESS;

fail:
    errno = -r;
    printf("Failed: %m\n");

    return EXIT_FAILURE;
}
