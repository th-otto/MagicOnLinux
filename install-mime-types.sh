#!/bin/bash
# This script registers Atari executable files with a MIME type and a respective icon.
# Must be run with sudo prefix.
xdg-mime install --novendor --mode system assets/atariprogram.xml
xdg-icon-resource install --context mimetypes --size 512 assets/atariprogram.png atariprogram
update-mime-database -V /usr/share/mime
update-icon-caches /usr/local/share/icons
update-desktop-database /usr/local/share/applications

#xdg-mime uninstall --novendor --mode system assets/atariprogram.xml

# if Hatari is installed, then add the following line:
#    <generic-icon name="application-x-st-disk-image"/>
# in
#    /usr/local/share/mime/packages/hatari.xml
# after
#    <glob pattern="*.st"/>
#
# otherwise:
#xdg-mime install --novendor --mode system assets/st-disk-image.xml
#xdg-icon-resource install --context mimetypes assets/application-x-st-disk-image.svg application-x-st-disk-image
#update-mime-database -V /usr/share/mime
#update-icon-caches /usr/local/share/icons
