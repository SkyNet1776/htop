#!/bin/sh
mkdir -p m4
autoreconf --install --force
cp Office.theme ~/.config/htop/Office.theme
cp Classic.theme ~/.config/htop/Classic.theme
