#!/bin/bash


gcc exporter.c xdg-shell-protocol.c \
    xdg-foreign-unstable-v2-client-protocol.c \
    -lwayland-client -o exporter


gcc importer.c xdg-shell-protocol.c \
    xdg-foreign-unstable-v2-client-protocol.c \
    -lwayland-client -o importer
