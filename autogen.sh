#!/bin/sh
mkdir -p m4
autoreconf --install --force
cp Office.theme ~/.config/htop/theme-tools/Office.theme
cp Classic.theme ~/.config/htop/theme=tools/Classic.theme
