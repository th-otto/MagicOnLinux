PROGRAMS = gxmessage magic-on-linux-settings

CC = gcc
CFLAGS = -O2 -Wall -W

all:: $(PROGRAMS)

gxmessage: gxmessage.c
	$(CC) $(CFLAGS) -o $@ `pkg-config --cflags --libs gtk+-2.0` $<

magic-on-linux-settings: magic-on-linux-settings.c preferences.xml.h
	$(CC) $(CFLAGS) -o $@ `pkg-config --cflags --libs gtk+-2.0` $<

preferences.xml.h: preferences.xml Makefile
	sed -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/"/' $< > $@

clean::
	$(RM) $(PROGRAMS) preferences.xml.h
