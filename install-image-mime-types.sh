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
xdg-icon-resource install --context mimetypes assets/application-x-st-disk-image.svg application-x-st-disk-image
update-mime-database -V /usr/share/mime
update-icon-caches /usr/local/share/icons

# This is the uninstall command:
#xdg-mime uninstall --novendor --mode system assets/st-disk-image.xml

