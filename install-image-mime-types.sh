#!/bin/bash
# This script registers Atari floppy disk files with a MIME type and a respective icon.
# Must be run with sudo prefix.

# if Hatari is installed, then add the following line:
#    <generic-icon name="application-x-st-disk-image"/>
# in
#    /usr/local/share/mime/packages/hatari.xml
# after
#    <glob pattern="*.st"/>
# if not already there.
#
# otherwise:

xdg-mime install --novendor --mode system assets/st-disk-image.xml
# Note that xdg-icon-resource cannot deal with .svg images. FWFR.
xdg-icon-resource install --context mimetypes --size 512 assets/application-x-st-disk-image.png application-x-st-disk-image
update-mime-database -V /usr/share/mime
update-icon-caches /usr/local/share/icons
