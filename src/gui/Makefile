CC = gcc
CXX = g++
WARN = -Wall -W -Werror
OPT = -O2
CFLAGS = $(OPT) $(WARN) -Wstrict-prototypes -Wmissing-prototypes
CXXFLAGS = $(OPT) $(WARN) -Wno-attributes
LDFLAGS = -s
YACC = bison -y
YFLAGS = -Wno-yacc

PROGRAMS = nlstool

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

NLSTOOL_OBJS = \
	nlstool.o pofile.o country.o \
	libnls/eval-plural-exp.o \
	libnls/eval-plural-str.o \
	libnls/plural-exp.o \
	libnls/plural.o \
	libnls/plurals.o \
	$(empty)

LIBNLS_OBJS = \
	libnls/dcgettext.o \
	libnls/dgettext.o \
	libnls/gettext.o \
	libnls/ngettext.o \
	libnls/dngettext.o \
	libnls/dcngettext.o \
	libnls/textdomain.o \
	libnls/setlocale.o \
	libnls/localename.o \
	libnls/eval-plural-exp.o \
	libnls/eval-plural-str.o \
	libnls/plural-exp.o \
	libnls/plural.o \
	libnls/plurals.o \
	$(empty)

all:: $(PROGRAMS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

.cc.o:
	$(CXX) $(CXXFLAGS) -c -o $@ $<

gxmessage: gxmessage.c $(MAKEFILE_LIST)
	$(CC) $(CFLAGS) -o $@ $(GTK_CFLAGS) $< $(GTK_LIBS) $(LDFLAGS)

mxgtk-settings: mxgtk-settings.c country.o preferences.xml.h langs.o libnls/libnls.a $(MAKEFILE_LIST)
	$(CC) $(CFLAGS) -o $@ $(GTK_CFLAGS) $< langs.o libnls/libnls.a $(GTK_LIBS) $(LDFLAGS)

preferences.xml.h: preferences.xml $(MAKEFILE_LIST)
	sed -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/"/' $< > $@

mxqt-settings:	mxqt-settings.cc country.o preferences.xml.h mxqt-settings.moc langs.o qrc.o libnls/libnls.a $(MAKEFILE_LIST)
	$(CXX) $(CXXFLAGS) -o $@ $(QT_CFLAGS) $< langs.o libnls/libnls.a $(QT_LIBS) $(LDFLAGS)

mxqt-settings.moc: mxqt-settings.cc $(MAKEFILE_LIST)
	$(MOC) -o$@ $<

qrc.o: qrc.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

qrc.cc: icons.qrc $(wildcard icons/*) $(MAKEFILE_LIST)
	$(RCC) --namespace -g cpp -o $@ $<

country.o: country.c
langs.o: langs.c

$(NLSTOOL_OBJS): pofile.h libnls/libnls.h $(MAKEFILE_LIST)

nlstool: $(NLSTOOL_OBJS)
	$(CC) $(CFLAGS) -o $@ $(NLSTOOL_OBJS) $(LDFLAGS)

langs.c: nlstool
	./nlstool -p ../../po --default-domain MagicOnLinux -o $@ || { $(RM) $@; false; }

libnls/libnls.a: $(LIBNLS_OBJS)
	@$(RM) $@
	ar rcs $@ $(LIBNLS_OBJS)

libnls/plural.o: libnls/plural.c

libnls/plural.c: libnls/plural.y
	$(YACC) $(YFLAGS) --output $@ $<

clean::
	$(RM) *.o libnls/*.o libnls/*.a $(PROGRAMS) preferences.xml.h *.moc qrc.cc
