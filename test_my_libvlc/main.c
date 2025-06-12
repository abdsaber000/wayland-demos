#include <vlc/vlc.h>
#include <vlc/libvlc_media_player.h>
#include <wayland-client.h>
#include <stdio.h>
#include <assert.h>

int main(int argc, char *argv[])
{
    char* file_path = "/mnt/01D97E09B2877ED0/anime/Hunter x Hunter (2011) - S01E148.mp4";
    const char* vlc_args[] = {
        "-vvv"
    };

    libvlc_instance_t *vlc = libvlc_new(1, vlc_args);
    if (!vlc) {
        fprintf(stderr, "Failed to create libVLC instance\n");
        return 1;
    }

    libvlc_media_t *media = libvlc_media_new_path(file_path);
    if (!media) {
        fprintf(stderr, "Failed to create media\n");
        libvlc_release(vlc);
        return 1;
    }
    libvlc_media_player_t *mp = libvlc_media_player_new_from_media(vlc, media);
    libvlc_media_release(media);

    if (!mp) {
        fprintf(stderr, "Failed to create media player\n");
        libvlc_release(vlc);
        return 1;
    }

    struct wl_surface *surface = (struct wl_surface *)0x12345678; // Dummy pointer for illustration

    libvlc_media_player_set_wayland_surface(mp, surface);
    // libvlc_media_player_set_xwindow(mp, 1000);
    struct wl_surface *surface2 = libvlc_media_player_get_wayland_surface(mp);
    
    assert(surface2 == surface);

    if (libvlc_media_player_play(mp) != 0) {
        fprintf(stderr, "Failed to play media\n");
    }

    printf("Playing... Press Enter to quit.\n");
    getchar();

    libvlc_media_player_stop_async(mp);
    libvlc_media_player_release(mp);
    libvlc_release(vlc);

    return 0;
}