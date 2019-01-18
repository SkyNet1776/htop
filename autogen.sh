#!/bin/sh
mkdir -p m4
autoreconf --install --force
mkdir ~/.config/htop  #temporary bandaid for including themes
cp theme-tools/Office.theme ~/.config/htop/Office.theme
cp theme-tools/Classic.theme ~/.config/htop/Classic.theme
