PROGRAMS = gxmessage magic-on-linux-settings

CC = gcc
CFLAGS = -O2 -Wall -W
GTK_CFLAGS = `pkg-config --cflags --libs gtk+-2.0`
# should also work:
#GTK_CFLAGS = `pkg-config --cflags --libs gtk+-3.0`
# does not work yet:
#GTK_CFLAGS = `pkg-config --cflags --libs gtk4`

all:: $(PROGRAMS)

gxmessage: gxmessage.c
	$(CC) $(CFLAGS) -o $@ $(GTK_CFLAGS) $<

magic-on-linux-settings: magic-on-linux-settings.c preferences.xml.h
	$(CC) $(CFLAGS) -o $@ $(GTK_CFLAGS) $<

preferences.xml.h: preferences.xml Makefile
	sed -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/"/' $< > $@

clean::
	$(RM) $(PROGRAMS) preferences.xml.h
