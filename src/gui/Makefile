CC = gcc
CXX = g++
CFLAGS = -O2 -Wall -W
CXXFLAGS = $(CFLAGS) -Wno-attributes
LDFLAGS = -s

PROGRAMS = 

# default values
GTK = 2
QT = 5

# GTK specific
ifeq ($(GTK),2)
GTK_PACKAGES = gtk+-2.0
endif
ifeq ($(GTK),3)
# should also work:
GTK_PACKAGES = gtk+-3.0
endif
ifeq ($(GTK),4)
# does not work yet:
GTK_PACKAGES = gtk4
endif
GTK_CFLAGS =    `pkg-config --cflags $(GTK_PACKAGES)`
GTK_LIBS =      `pkg-config --libs $(GTK_PACKAGES)`
ifneq ($(GTK_LIBS),)
PROGRAMS += gxmessage mxgtk-settings
endif

# Qt specific
ifeq ($(QT),5)
MOC =           moc-qt5
RCC =           rcc-qt5
QT_PACKAGES =   Qt5Core Qt5Gui Qt5Widgets
endif
ifeq ($(QT),6)
MOC =           /usr/libexec/qt6/moc
RCC =           /usr/libexec/qt6/rcc
QT_PACKAGES =   Qt6Core Qt6Gui Qt6Widgets
endif
QT_CFLAGS =     `pkg-config --cflags $(QT_PACKAGES)`
QT_LIBS =       `pkg-config --libs $(QT_PACKAGES)`
ifneq ($(QT_LIBS),)
PROGRAMS += mxqt-settings
endif


all:: $(PROGRAMS)

gxmessage: gxmessage.c $(MAKEFILE_LIST)
	$(CC) $(CFLAGS) -o $@ $(GTK_CFLAGS) $< $(GTK_LIBS) $(LDFLAGS)

mxgtk-settings: mxgtk-settings.c country.c preferences.xml.h $(MAKEFILE_LIST)
	$(CC) $(CFLAGS) -o $@ $(GTK_CFLAGS) $< $(GTK_LIBS) $(LDFLAGS)

preferences.xml.h: preferences.xml $(MAKEFILE_LIST)
	sed -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/"/' $< > $@

mxqt-settings:	mxqt-settings.cc country.c preferences.xml.h mxqt-settings.moc qrc.cc $(MAKEFILE_LIST)
	$(CXX) $(CXXFLAGS) -o $@ $(QT_CFLAGS) $< $(QT_LIBS) $(LDFLAGS)

mxqt-settings.moc: mxqt-settings.cc $(MAKEFILE_LIST)
	$(MOC) -o$@ $<

qrc.cc: icons.qrc $(wildcard icons/*) $(MAKEFILE_LIST)
	$(RCC) --namespace -g cpp -o $@ $<

clean::
	$(RM) $(PROGRAMS) preferences.xml.h *.moc qrc.cc
