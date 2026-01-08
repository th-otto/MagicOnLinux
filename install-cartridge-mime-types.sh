#!/bin/bash
# This script registers Atari ST ROM cartridge images (4 bytes Null header, 128 k data) with a MIME type and a respective icon.
# Must be run with sudo prefix.
xdg-mime install --novendor --mode system assets/atari-st-rom-cartridge.xml
xdg-icon-resource install --context mimetypes --size 512 assets/atari-st-rom-cartridge.png atari-st-rom-cartridge
update-mime-database -V /usr/share/mime
update-icon-caches /usr/local/share/icons
update-desktop-database /usr/local/share/applications
