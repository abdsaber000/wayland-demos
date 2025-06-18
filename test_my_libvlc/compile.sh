gcc  main.c xdg-shell-protocol.c xdg-foreign-unstable-v2-client-protocol.c $(PKG_CONFIG_PATH=/home/abdo/vlc/build-lib/install/lib/pkgconfig pkg-config --libs --cflags libvlc) \
    -lwayland-client \
    -g \
    -o main

LD_LIBRARY_PATH=/home/abdo/vlc/build-lib/install/lib 
# LD_LIBRARY_PATH=/home/abdo/Desktop/vlc/lib/.libs/:/home/abdo/Desktop/vlc/src/.libs/ 
# VLC_PLUGIN_PATH=/home/abdo/Desktop/vlc/modules/.libs  
gdb ./main
# ./main