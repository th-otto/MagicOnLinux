#!/bin/bash
# This script registers Atari executable files with a MIME type and a respective icon.
# Must be run with sudo prefix.
xdg-mime install --novendor --mode system assets/atariprogram.xml
xdg-icon-resource install --context mimetypes --size 512 assets/atariprogram.png atariprogram
update-mime-database -V /usr/share/mime
update-icon-caches /usr/local/share/icons
update-desktop-database /usr/local/share/applications

# This is the uninstall command:
#xdg-mime uninstall --novendor --mode system assets/atariprogram.xml

