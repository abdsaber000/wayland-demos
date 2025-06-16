#!/bin/bash

gcc main.c xdg-shell-protocol.c \
    -lwayland-client -o main


