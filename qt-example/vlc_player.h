#pragma once

#include <string>
#include <vlc/libvlc.h>
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_renderer_discoverer.h>
#include <vlc/libvlc_media_player.h>
#include <vlc/vlc.h>
#include <wayland-client.h>
#include <iostream>

struct RenderParams {
    wl_display *display;
    wl_surface *surface;
    int width;
    int height;
};

libvlc_video_output_resize_cb report_size_change;
void *opaque;

void set_callbacks(void* data,
        libvlc_video_output_resize_cb report_size_change_,
        libvlc_video_output_mouse_move_cb report_mouse_move_,
        libvlc_video_output_mouse_press_cb report_mouse_press_,
        libvlc_video_output_mouse_release_cb report_mouse_release_,
        void* reportOpaqu) {
    report_size_change = report_size_change_;
    opaque = reportOpaqu;
    printf("report opaque %p\n", reportOpaqu);
    report_size_change(opaque, 800, 600); 
}

class VlcPlayer {
public:
    VlcPlayer(int argc, const char *const *argv) {
        m_vlc = libvlc_new(argc, argv);
        m_mp = libvlc_media_player_new(m_vlc);
    }

    ~VlcPlayer() {
        libvlc_media_player_release(m_mp);
        libvlc_release(m_vlc);
    }

    void set_render_window(const RenderParams &params) {
        std::cout << "Setting render window with display: " 
                  << params.display << ", surface: " 
                  << params.surface << ", width: " 
                  << params.width << ", height: " 
                  << params.height << std::endl;
        libvlc_media_player_set_wayland_surface(m_mp, 
            params.display, params.surface, set_callbacks);
    }

    void open_media(const std::string &path) {
        libvlc_media_t *media = libvlc_media_new_path(path.c_str());
        m_mp = libvlc_media_player_new_from_media(m_vlc, media);
        libvlc_media_release(media);
    }

    void play() { libvlc_media_player_play(m_mp); }
    
    void set_size(int width, int height) {
        if (report_size_change != NULL) {
            report_size_change(opaque, width, height);
        }
    }

private:

    libvlc_instance_t *m_vlc;
    libvlc_media_player_t *m_mp;
    

};