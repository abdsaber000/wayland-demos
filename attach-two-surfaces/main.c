#include <wayland-client.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "xdg-shell-client-header.h"

struct state {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct xdg_wm_base *wm_base;

    struct wl_surface *parent_surface;
    struct wl_surface *child_surface;
    struct xdg_surface *parent_xdg_surface;
    struct xdg_toplevel *parent_toplevel;
    struct xdg_surface *child_xdg_surface;
    struct xdg_toplevel *child_toplevel;

    struct wl_buffer *parent_buffer;
    struct wl_buffer *child_buffer;
    uint32_t *parent_shm_data;
    uint32_t *child_shm_data;
    int parent_width, parent_height;
    int child_width, child_height;
};



static int create_shm_file(size_t size) {
    char name[8];
    name[0] = '/';
    name[7] = '\0';
    for (int i = 1; i < 7; i++) {
        name[i] = 'a' + (rand() % 26); 
    }
    int fd = shm_open(name, O_CREAT | O_RDWR, 0600);
    shm_unlink(name);
    if (fd < 0) {
        perror("shm_open");
        return -1;
    }

    if (ftruncate(fd, size) < 0) {
        perror("ftruncate");
        close(fd);
        return -1;
    }
    return fd;
}

static struct wl_buffer *create_buffer(struct state *state, int width, int height, uint32_t **data_out) {
    int stride = width * 4;
    int size = stride * height;
    int fd = create_shm_file(size);
    if (fd < 0) return NULL;

    uint32_t *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return NULL;
    }
    struct wl_shm_pool *pool = wl_shm_create_pool(state->shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    // Fill with color (parent: blue, child: green)
    uint32_t color = (width == state->parent_width && height == state->parent_height) ? 0xFF0000FF : 0xFF00FF00;
    for (int i = 0; i < width * height; ++i) data[i] = color;

    if (data_out) *data_out = data;
    return buffer;
}

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial) {
    xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static void parent_xdg_surface_configure(void *data, struct xdg_surface *surface, uint32_t serial) {
    struct state *state = data;
    xdg_surface_ack_configure(surface, serial);

    // Recreate parent buffer
    if (state->parent_buffer) wl_buffer_destroy(state->parent_buffer);
    state->parent_buffer = create_buffer(state, state->parent_width, state->parent_height, &state->parent_shm_data);
    wl_surface_attach(state->parent_surface, state->parent_buffer, 0, 0);
    wl_surface_damage_buffer(state->parent_surface, 0, 0, state->parent_width, state->parent_height);
    wl_surface_commit(state->parent_surface);

    
}

static const struct xdg_surface_listener parent_xdg_surface_listener = {
    .configure = parent_xdg_surface_configure,
};

static void child_xdg_surface_configure(void *data, struct xdg_surface *surface, uint32_t serial) {
    xdg_surface_ack_configure(surface, serial);
    struct state *state = data;
    // Resize child to match parent (for demonstration)
    state->child_width = state->parent_width / 2;
    state->child_height = state->parent_height / 2;

    // // Recreate child buffer
    if (state->child_buffer) wl_buffer_destroy(state->child_buffer);
    state->child_buffer = create_buffer(state, state->child_width, state->child_height, &state->child_shm_data);
    wl_surface_attach(state->child_surface, state->child_buffer, 0, 0);
    wl_surface_damage_buffer(state->child_surface, 0, 0, state->child_width, state->child_height);
    wl_surface_commit(state->child_surface);
}

static const struct xdg_surface_listener child_xdg_surface_listener = {
    .configure = child_xdg_surface_configure,
};

static void registry_global(void *data, struct wl_registry *registry,
                           uint32_t name, const char *interface, uint32_t version) {
    struct state *state = data;
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        state->wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(state->wm_base, &wm_base_listener, state);
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

int main() {
    struct state state = {0};
    state.parent_width = 400;
    state.parent_height = 400;
    state.child_width = 200;
    state.child_height = 200;

    state.display = wl_display_connect(NULL);
    if (!state.display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return 1;
    }
    state.registry = wl_display_get_registry(state.display);
    wl_registry_add_listener(state.registry, &registry_listener, &state);
    wl_display_roundtrip(state.display);

    if (!state.compositor || !state.shm || !state.wm_base) {
        fprintf(stderr, "Missing required globals\n");
        return 1;
    }

    // Parent surface
    state.parent_surface = wl_compositor_create_surface(state.compositor);
    state.parent_xdg_surface = xdg_wm_base_get_xdg_surface(state.wm_base, state.parent_surface);
    xdg_surface_add_listener(state.parent_xdg_surface, &parent_xdg_surface_listener, &state);
    state.parent_toplevel = xdg_surface_get_toplevel(state.parent_xdg_surface);
    xdg_toplevel_set_title(state.parent_toplevel, "Parent");
    xdg_toplevel_set_app_id(state.parent_toplevel, "parent");

    // Child surface
    state.child_surface = wl_compositor_create_surface(state.compositor);
    state.child_xdg_surface = xdg_wm_base_get_xdg_surface(state.wm_base, state.child_surface);
    xdg_surface_add_listener(state.child_xdg_surface, &child_xdg_surface_listener, &state);
    state.child_toplevel = xdg_surface_get_toplevel(state.child_xdg_surface);
    xdg_toplevel_set_title(state.child_toplevel, "Child");
    xdg_toplevel_set_app_id(state.child_toplevel, "child");

    // // Set child as a subsurface of parent (for stacking, not parenting in xdg-shell)
    // // For true parent-child in xdg-shell, use xdg_toplevel_set_parent:
    xdg_toplevel_set_parent(state.child_toplevel, state.parent_toplevel);

    wl_surface_commit(state.parent_surface);
    wl_display_roundtrip(state.display);
    // wl_surface_commit(state.child_surface);
    // wl_display_roundtrip(state.display);

    // Enter event loop
    while (wl_display_dispatch(state.display) != -1) {}

    wl_display_disconnect(state.display);
    return 0;
}