#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#include "xdg-foreign-unstable-v2-client-protocol.h"
#include "xdg-shell-client-header.h"

struct app_state {
    struct wl_compositor *compositor;
    struct xdg_wm_base *wm_base;
    struct zxdg_importer_v2 *importer;
    struct zxdg_imported_v2 *imported;
    struct wl_shm *shm;

    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *toplevel;
    struct wl_buffer *buffer;

    uint8_t *shm_data;
    int width, height;
    int running;
    int configured;
};

void handle_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial) {
    xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = handle_ping,
};

int create_shm_buffer(struct app_state *state) {
    state->width = 400;
    state->height = 400;
    int stride = state->width * 4;
    int size = stride * state->height;

    int fd = shm_open("/myshm", O_CREAT | O_RDWR, 0600);
    shm_unlink("/myshm");
    if (fd < 0) {
        perror("shm_open");
        return -1;
    }

    if (ftruncate(fd, size) < 0) {
        perror("ftruncate");
        close(fd);
        return -1;
    }

    state->shm_data =
        mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (state->shm_data == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return -1;
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(state->shm, fd, size);
    state->buffer = wl_shm_pool_create_buffer(
        pool, 0, state->width, state->height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    memset(state->shm_data, 0xFF, size);
    return 0;
}

void xdg_surface_configure(void *data, struct xdg_surface *surface,
                           uint32_t serial) {
    struct app_state *state = data;
    xdg_surface_ack_configure(surface, serial);

    if (!state->configured) {
        state->configured = 1;

        if (create_shm_buffer(state) < 0) {
            fprintf(stderr, "Failed to create SHM buffer\n");
            return;
        }
        wl_surface_attach(state->surface, state->buffer, 0, 0);
        wl_surface_commit(state->surface);
    }
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

void xdg_toplevel_configure(void *data, struct xdg_toplevel *toplevel,
                            int32_t width, int32_t height,
                            struct wl_array *states) {}

void handle_toplevel_close(void *data, struct xdg_toplevel *toplevel) {
    struct app_state *state = data;
    state->running = 0;
}

static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = xdg_toplevel_configure, .close = handle_toplevel_close};

void handle_imported_destroyed(void *data, struct zxdg_imported_v2 *imported) {
    struct app_state *state = data;
    state->imported = NULL;
}

static const struct zxdg_imported_v2_listener imported_listener = {
    .destroyed = handle_imported_destroyed,
};

// Create window with proper lifecycle management
static void create_window(struct app_state *state, const char *import_handle) {
    state->surface = wl_compositor_create_surface(state->compositor);
    state->xdg_surface =
        xdg_wm_base_get_xdg_surface(state->wm_base, state->surface);
    xdg_surface_add_listener(state->xdg_surface, &xdg_surface_listener, state);

    state->toplevel = xdg_surface_get_toplevel(state->xdg_surface);
    xdg_toplevel_add_listener(state->toplevel, &toplevel_listener, state);
    xdg_toplevel_set_title(state->toplevel, "IMPORTER (Child Window)");

    // Import handle AFTER surface creation
    state->imported =
        zxdg_importer_v2_import_toplevel(state->importer, import_handle);
    if (!state->imported) {
        fprintf(stderr, "Failed to import handle: %s\n", import_handle);
        return;
    }
    zxdg_imported_v2_add_listener(state->imported, &imported_listener, state);

    // Set parent relationship
    zxdg_imported_v2_set_parent_of(state->imported, state->surface);
    wl_surface_commit(state->surface);
}

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface,
                            uint32_t version) {
    struct app_state *state = data;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->compositor =
            wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        state->wm_base =
            wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(state->wm_base, &wm_base_listener, NULL);
    } else if (strcmp(interface, zxdg_importer_v2_interface.name) == 0) {
        state->importer =
            wl_registry_bind(registry, name, &zxdg_importer_v2_interface, 1);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    }
}

static void global_remove(void *data, struct wl_registry *registry,
                          uint32_t name) {}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <exported-handle>\n", argv[0]);
        return 1;
    }

    struct app_state state = {0};
    state.running = 1;

    struct wl_display *display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return 1;
    }

    struct wl_registry *registry = wl_display_get_registry(display);
    static const struct wl_registry_listener registry_listener = {
        .global = registry_global,
        .global_remove = global_remove,
    };
    wl_registry_add_listener(registry, &registry_listener, &state);
    wl_display_roundtrip(display);

    if (!state.compositor || !state.wm_base || !state.importer) {
        fprintf(stderr, "Missing required Wayland globals\n");
        return 1;
    }

    create_window(&state, argv[1]);

    while (state.running && wl_display_dispatch(display) != -1) {
    }

    if (state.imported) {
        zxdg_imported_v2_destroy(state.imported);
    }
    if (state.toplevel) {
        xdg_toplevel_destroy(state.toplevel);
    }
    if (state.xdg_surface) {
        xdg_surface_destroy(state.xdg_surface);
    }
    if (state.surface) {
        wl_surface_destroy(state.surface);
    }
    if (state.buffer) {
        wl_buffer_destroy(state.buffer);
    }
    if (state.shm_data) {
        munmap(state.shm_data, state.width * state.height * 4);
    }

    if (state.importer) zxdg_importer_v2_destroy(state.importer);
    if (state.wm_base) xdg_wm_base_destroy(state.wm_base);
    if (state.compositor) wl_compositor_destroy(state.compositor);
    if (state.shm) wl_shm_destroy(state.shm);

    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    return 0;
}