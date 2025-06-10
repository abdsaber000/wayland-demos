#!/bin/bash

gcc first.c xdg-shell-protocol.c \
    -lwayland-client -o first

gcc second.c xdg-shell-protocol.c \
    -lwayland-client -o second
