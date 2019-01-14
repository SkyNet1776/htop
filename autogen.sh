#!/bin/sh
mkdir -p m4
autoreconf --install --force
cp Office.theme ~/.config/htop/Office.theme
