#include <wayland-client.h>
#include "xdg-shell-client-header.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>

#define FILE_NAME "./share_data.txt"

struct state {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct xdg_wm_base *wm_base;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *toplevel;

    struct wl_shm *shm;
    struct wl_buffer *buffer;
    uint32_t *shm_data;
    int shm_size;
    FILE* file;
    int width, height;
};

int create_shm_buffer(struct state *state) {
    int stride = state->width * 4;
    int size = stride * state->height;

    int fd = shm_open("/myshmf", O_CREAT | O_RDWR, 0600);
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

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial) {
    xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static void xdg_surface_configure(void *data, struct xdg_surface *surface, uint32_t serial) {
    struct state *state = data;
    xdg_surface_ack_configure(surface, serial);

    if (create_shm_buffer(state) < 0) {
            fprintf(stderr, "Failed to create SHM buffer\n");
            return;
        }
    wl_surface_attach(state->surface, state->buffer, 0, 0);
    wl_surface_commit(state->surface);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void xdg_toplevel_configure(void *data, struct xdg_toplevel *toplevel,
                                   int32_t width, int32_t height,
                                   struct wl_array *states) {
    struct state *state = data;
    if (width > 0 && height > 0) {
        state->width = width;
        state->height = height;
        printf("Resizing to %dx%d\n", width, height);
        fseek(state->file, 0, SEEK_SET);
        fprintf(state->file,"%d\n%d\n", state->height, state->width);
        fflush(state->file);
    }
}

static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = xdg_toplevel_configure,
};

static void registry_global(void *data, struct wl_registry *registry,
                           uint32_t name, const char *interface,
                           uint32_t version) {
    struct state *state = data;
    
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->compositor = wl_registry_bind(
            registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        state->wm_base = wl_registry_bind(
            registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(state->wm_base, &wm_base_listener, NULL);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    }
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
};


int main() {
    struct state state = {0};
    state.width = 400;
    state.height = 400;
    state.file = fopen(FILE_NAME, "w");
    // Connect to Wayland
    state.display = wl_display_connect(NULL);
    if (!state.display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return 1;
    }
    
    state.registry = wl_display_get_registry(state.display);
    wl_registry_add_listener(state.registry, &registry_listener, &state);
    wl_display_roundtrip(state.display);
    
    if (!state.compositor || !state.wm_base) {
        fprintf(stderr, "Missing required globals\n");
        return 1;
    }
    
    // Create window
    state.surface = wl_compositor_create_surface(state.compositor);
    state.xdg_surface = xdg_wm_base_get_xdg_surface(state.wm_base, state.surface);
    xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, &state);
    state.toplevel = xdg_surface_get_toplevel(state.xdg_surface);
    xdg_toplevel_add_listener(state.toplevel, &toplevel_listener, &state);
    xdg_toplevel_set_title(state.toplevel, "Controller Window");
    xdg_toplevel_set_app_id(state.toplevel, "controller");
    
    // Set initial size
    wl_surface_commit(state.surface);

    wl_display_roundtrip(state.display); // Ensure shm is bound
    wl_surface_attach(state.surface, state.buffer, 0, 0);
    wl_surface_damage_buffer(state.surface, 0, 0, state.width, state.height);
    wl_surface_commit(state.surface);
    
    // Main loop
    while (wl_display_dispatch(state.display) != -1) {
        // Keep handling events
    }
    
    fclose(state.file);
    wl_display_disconnect(state.display);
    return 0;
}