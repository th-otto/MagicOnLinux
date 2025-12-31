#!/bin/bash
# This script unregisters MIME types, removes icons and application.
# Must be run with sudo prefix.
xdg-mime uninstall --novendor --mode system assets/atariprogram.xml
xdg-icon-resource uninstall --context mimetypes --size 512 atariprogram
xdg-mime uninstall --novendor --mode system assets/st-disk-image.xml
xdg-icon-resource uninstall --context mimetypes application-x-st-disk-image
rm /usr/local/share/applications/magic-on-linux.desktop
rm /usr/local/share/icons/hicolor/512x512/apps/magic-on-linux.png
update-mime-database -V /usr/share/mime
update-icon-caches /usr/local/share/icons
update-desktop-database /usr/local/share/applications
