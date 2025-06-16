#include <vlc/vlc.h>
#include <vlc/libvlc_media_player.h>
#include <wayland-client.h>
#include <stdio.h>
#include <assert.h>

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <wayland-client.h>

#include "xdg-foreign-unstable-v2-client-protocol.h"
#include "xdg-shell-client-header.h"

struct wl_display *display;
struct wl_compositor *compositor;
struct wl_surface *surface;
struct wl_buffer *buffer;
struct wl_shm *shm;
struct xdg_wm_base *xdg_wm_base;
struct xdg_toplevel *toplevel;
struct wl_seat *seat;
struct wl_keyboard *keyboard;
struct wl_pointer *pointer;
struct xdg_surface *xdg_surface;
struct zxdg_exporter_v2 *exporter = NULL;
struct zxdg_exported_v2 *exported = NULL;
char *exported_handle = NULL;
uint8_t *shm_data;
int16_t width = 500;
int16_t height = 500;
int8_t color = 0;
uint8_t close_flag = 0;
uint32_t first_color = 0xFF666666;
uint32_t second_color = 0xFFEEEEEE;

///// vlc /////////
libvlc_instance_t *vlc;
libvlc_media_t *media;
libvlc_media_player_t *mp;
libvlc_video_output_resize_cb report_size_change;
//////////////////

void set_callbacks(void* data,
        libvlc_video_output_resize_cb report_size_change_,
        libvlc_video_output_mouse_move_cb report_mouse_move_,
        libvlc_video_output_mouse_press_cb report_mouse_press_,
        libvlc_video_output_mouse_release_cb report_mouse_release_,
        void* reportOpaqu) {
    
    report_size_change = report_size_change_;
    
}


void handle_exported(void *data, struct zxdg_exported_v2 *zxdg_exported_v2,
                     const char *handle) {
    exported_handle = strdup(handle);
    printf("Handle: %s\n", exported_handle);
    libvlc_media_player_set_wayland_surface(mp, handle, display, surface);

    // char* handle2 = libvlc_media_player_get_wayland_surface(mp);
    
    // assert(handle == handle2);

    // if (libvlc_media_player_play(mp) != 0) {
    //     fprintf(stderr, "Failed to play media\n");
    // }

}

struct zxdg_exported_v2_listener exported_listener = {.handle =
                                                          handle_exported};

int allocate_shm(uint64_t size) {
    int8_t name[8];
    name[0] = '/';
    name[7] = 0;
    for (int i = 1; i < 7; i++) {
        name[i] = 'a' + (rand() % 26);
    }
    int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL,
                      S_IWUSR | S_IRUSR | S_IWOTH | S_IROTH);

    if (fd < 0) {
        perror("shm_open");
        return -1;
    }
    shm_unlink(name);
    if (ftruncate(fd, size) == -1) {
        perror("ftruncate");
        close(fd);
        return -1;
    }

    return fd;
}

void resize() {
    int fd = allocate_shm(width * height * 4);
    if (fd < 0) {
        return;
    }
    shm_data = mmap(NULL, width * height * 4, PROT_READ | PROT_WRITE,
                    MAP_SHARED, fd, 0);

    if (shm_data == MAP_FAILED) {
        perror("mmap");
        close(fd);
        shm_data = NULL;
        return;
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, width * height * 4);
    buffer = wl_shm_pool_create_buffer(pool, 0, width, height, width * 4,
                                       WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    pool = NULL;
    close(fd);
}

void invert_chess_board_colors() {
    uint32_t tmp = first_color;
    first_color = second_color;
    second_color = tmp;
}

void draw_chess_board() {
    uint32_t *pixels = (uint32_t *)shm_data;
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            if ((x + y / 8 * 8) % 16 < 8) {
                pixels[y * width + x] = first_color;
            } else {
                pixels[y * width + x] = second_color;
            }
        }
    }
}

void draw() {
    // memset(shm_data, color, width * height * 4);

    draw_chess_board();

    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage_buffer(surface, 0, 0, width, height);
    wl_surface_commit(surface);
}

void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
                           uint32_t serial) {
    xdg_surface_ack_configure(xdg_surface, serial);
    if (!shm_data) {
        resize();
    }
    draw();
}

struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};


void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
                            int32_t new_width, int32_t new_height,
                            struct wl_array *states) {
    if (new_width <= 0 || new_height <= 0) {
        return;
    }
    if (width != new_width || height != new_height) {
        munmap(shm_data, width * height * 4);
        width = new_width;
        height = new_height;
        resize();
    }
    report_size_change(NULL, width, height);
}

void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    close_flag = 1;
}

struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure, .close = xdg_toplevel_close};

void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base,
                      uint32_t serial) {
    xdg_wm_base_pong(xdg_wm_base, serial);
}

struct xdg_wm_base_listener xdg_wm_base_listener = {.ping = xdg_wm_base_ping};

void keyboard_enter(void *data, struct wl_keyboard *wl_keyboard,
                    uint32_t serial, struct wl_surface *surface,
                    struct wl_array *keys) {}




void keyboard_key(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial,
                  uint32_t time, uint32_t key, uint32_t state) {
    if (key == 1) {  // escape character
        close_flag = 1;
    } 
}

void keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard,
                     uint32_t format, int32_t fd, uint32_t size) {}

void keyboard_leave(void *data, struct wl_keyboard *wl_keyboard,
                    uint32_t serial, struct wl_surface *surface) {}

void keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard,
                        uint32_t serial, uint32_t mods_depressed,
                        uint32_t mods_latched, uint32_t mods_locked,
                        uint32_t group) {}

void keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
                          int32_t rate, int32_t delay) {}

struct wl_keyboard_listener keyboard_listener = {
    .enter = keyboard_enter,
    .key = keyboard_key,
    .keymap = keyboard_keymap,
    .leave = keyboard_leave,
    .modifiers = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info};


void pointer_enter(void *data, struct wl_pointer *wl_pointer, uint32_t serial,
                   struct wl_surface *surface, wl_fixed_t sx, wl_fixed_t sy) {
    // Pointer entered the surface
}

void pointer_leave(void *data, struct wl_pointer *wl_pointer, uint32_t serial,
                   struct wl_surface *surface) {
    // Pointer left the surface
}

void pointer_motion(void *data, struct wl_pointer *wl_pointer, uint32_t time,
                    wl_fixed_t sx, wl_fixed_t sy) {
    // Pointer moved
}

void pointer_button(void *data, struct wl_pointer *wl_pointer, uint32_t serial,
                    uint32_t time, uint32_t button, uint32_t state) {
    if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
        printf("mouse clicked!!\n");
        invert_chess_board_colors();
    }
}

void pointer_axis(void *data, struct wl_pointer *wl_pointer, uint32_t time,
                  uint32_t axis, wl_fixed_t value) {
    // Pointer axis event (e.g., scroll)
}

struct wl_pointer_listener pointer_listener = {
    .enter = pointer_enter,
    .leave = pointer_leave,
    .motion = pointer_motion,
    .button = pointer_button,
    .axis = pointer_axis,
};
void seat_capabilities(void *data, struct wl_seat *seat,
                       uint32_t capabilities) {
    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && !keyboard) {
        keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(keyboard, &keyboard_listener, NULL);
    }

    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !pointer) {
        pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(pointer, &pointer_listener, NULL);
    }

    if (!(capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && keyboard) {
        wl_keyboard_destroy(keyboard);
        keyboard = NULL;
    }

    if (!(capabilities & WL_SEAT_CAPABILITY_POINTER) && pointer) {
        wl_pointer_destroy(pointer);
        pointer = NULL;
    }
}

void seat_name(void *data, struct wl_seat *seat, const char *name) {}

struct wl_seat_listener seat_listener = {.capabilities = seat_capabilities,
                                         .name = seat_name};

void registry_global(void *data, struct wl_registry *registry, uint32_t id,
                     const char *interface, uint32_t version) {
    if (!strcmp(interface, wl_compositor_interface.name)) {
        compositor =
            wl_registry_bind(registry, id, &wl_compositor_interface, 4);
    } else if (!strcmp(interface, wl_shm_interface.name)) {
        shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
    } else if (!strcmp(interface, xdg_wm_base_interface.name)) {
        xdg_wm_base = wl_registry_bind(registry, id, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(xdg_wm_base, &xdg_wm_base_listener, NULL);
    } else if (!strcmp(interface, wl_seat_interface.name)) {
        seat = wl_registry_bind(registry, id, &wl_seat_interface, 1);
        wl_seat_add_listener(seat, &seat_listener, NULL);
    } else if (!strcmp(interface, zxdg_exporter_v2_interface.name)) {
        exporter =
            wl_registry_bind(registry, id, &zxdg_exporter_v2_interface, 1);
    }
}

void registry_global_remove(void *data, struct wl_registry *registry,
                            uint32_t id) {
    printf("Global remove: %u\n", id);
}

struct wl_registry_listener listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

void clean_up() {
    printf("in clean up\n");
    fflush(stdout);
    
    if (exported) {
        zxdg_exported_v2_destroy(exported);
        exported = NULL;
    }
    if (toplevel) {
        xdg_toplevel_destroy(toplevel);
        toplevel = NULL;
    }
    if (xdg_surface) {
        xdg_surface_destroy(xdg_surface);
        xdg_surface = NULL;
    }
    if (surface) {
        wl_surface_destroy(surface);
        surface = NULL;
    }
    if (buffer) {
        wl_buffer_destroy(buffer);
        buffer = NULL;
    }
    if (shm_data) {
        munmap(shm_data, width * height * 4);
        shm_data = NULL;
    }
    if (keyboard) {
        wl_keyboard_destroy(keyboard);
        keyboard = NULL;
    }
    if (pointer) {
        wl_pointer_destroy(pointer);
        pointer = NULL;
    }
    if (seat) {
        wl_seat_destroy(seat);
        seat = NULL;
    }
    if (exporter) {
        zxdg_exporter_v2_destroy(exporter);
        exporter = NULL;
    }
    if (exported_handle) {
        free(exported_handle);
        exported_handle = NULL;
    }
}

void window_init() {
    surface = wl_compositor_create_surface(compositor);
    
    xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);

    toplevel = xdg_surface_get_toplevel(xdg_surface);
    xdg_toplevel_add_listener(toplevel, &xdg_toplevel_listener, NULL);
    xdg_toplevel_set_title(toplevel, "Hello Wayland");

}

void handle_sigint(int sig) {
    printf("Received signal %d, cleaning up...\n", sig);
    close_flag = 1;
}

void init_vlc() {
    char *file_path = "/mnt/01D97E09B2877ED0/anime/Hunter x Hunter (2011) - S01E148.mp4";
    const char* vlc_args[] = {
        "-vvv"
    };

    vlc = libvlc_new(1, vlc_args);
    if (!vlc) {
        fprintf(stderr, "Failed to create libVLC instance\n");
        return ;
    }

    media = libvlc_media_new_path(file_path);
    if (!media) {
        fprintf(stderr, "Failed to create media\n");
        libvlc_release(vlc);
        return ;
    }
    mp = libvlc_media_player_new_from_media(vlc, media);
    libvlc_media_release(media);

    if (!mp) {
        fprintf(stderr, "Failed to create media player\n");
        libvlc_release(vlc);
        return ;
    }

    libvlc_video_set_output_callbacks(mp, 
        libvlc_video_engine_opengl, NULL, set_callbacks, NULL, NULL,
                                     NULL, NULL, NULL, NULL, NULL, NULL);
}

int main(int argc, char *argv[])
{
    

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    init_vlc();

    display = wl_display_connect(NULL);
    if (display == NULL) {
        printf("Failed to connect to Wayland display\n");
        return -1;
    }
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &listener, NULL);
    wl_display_roundtrip(display);

    window_init();
    exported = zxdg_exporter_v2_export_toplevel(exporter, surface);
    zxdg_exported_v2_add_listener(exported, &exported_listener, NULL);

    wl_surface_commit(surface);


    while (wl_display_dispatch(display)) {
        if (close_flag) {
            break;
        }
    }
    wl_display_roundtrip(display);
    if (surface && buffer) {
        wl_surface_attach(surface, NULL, 0, 0);
        wl_surface_commit(surface);
        wl_display_roundtrip(display);
    }
    clean_up();
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    printf("reached the end of exporter.\n");
    fflush(stdout);
    libvlc_media_player_stop_async(mp);
    libvlc_media_player_release(mp);
    libvlc_release(vlc);

    return 0;
}