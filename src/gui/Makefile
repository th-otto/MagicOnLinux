CC = gcc
CXX = g++
WARN = -Wall -W -Werror
OPT = -O2
DEFS = -I/opt/local/include
CFLAGS = $(OPT) $(WARN) $(DEFS) -Wdeclaration-after-statement -Wstrict-prototypes -Wmissing-prototypes
CXXFLAGS = $(OPT) $(WARN) $(DEFS) -Wno-attributes
LDFLAGS = -s
YACC = bison -y
YFLAGS = -Wno-yacc
PERL = perl

srcdir = .

ENABLE_NLS = yes
PO_DIR = ../../po
GETTEXT_PACKAGE = MagicOnLinux
# some versions of make will truncate the line after #, some don't
hash = \#
POTFILES = $(notdir $(shell grep -v '^$(hash)' $(PO_DIR)/POTFILES))

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

ifeq ($(ENABLE_NLS),yes)
DEFS += -DENABLE_NLS
endif

NLSTOOL_OBJS = \
	nlstool.o pofile.o country.o langs.o \
	$(empty)

NLSTOOL_NONLS_OBJS = \
	nlstool-nonls.o pofile-nonls.o country.o \
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
	libnls/plurals.o \
	libnls/expreval.o \
	libnls/expreval-exp.o \
	libnls/expreval-str.o \
	libnls/expreval-silent.o \
	libnls/expreval-print.o \
	libnls/expreval-node.o \
	$(empty)

all:: $(PROGRAMS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

.cc.o:
	$(CXX) $(CXXFLAGS) -c -o $@ $<

FLAG_ICONS = $(wildcard icons/*.png)

define ICON_TEMPLATE
$(1).h: $(1) gen-pixbuf-csource
	./gen-pixbuf-csource --raw --extern --name=$(subst -,_,$(notdir $(basename $(1))))_data $(1) > $$@.tmp && mv $$@.tmp $$@ || { $(RM) $$@.tmp; false; }

endef
$(foreach ICON,$(FLAG_ICONS),$(eval $(call ICON_TEMPLATE,$(ICON))))

gen-pixbuf-csource: $(srcdir)/gen-pixbuf-csource.c
	$(CC) $(CFLAGS) -o $@ $(GTK_CFLAGS) $< $(GTK_LIBS) $(LDFLAGS)

gtkrc.o: gtkrc.c $(addsuffix .h,$(FLAG_ICONS))
	$(CC) $(CFLAGS) -c -o $@ $<

ifeq ($(ENABLE_NLS),yes)
gxmessage: gxmessage.tr.c $(MAKEFILE_LIST)
	$(CC) $(CFLAGS) -DNLS_TRANSLATE_TR -o $@ $(GTK_CFLAGS) $< $(GTK_LIBS) $(LDFLAGS)

mxgtk-settings: mxgtk-settings.tr.c country.o preferences.xml.h langs.o libnls/libnls.a preferences.xml.tr.c gtkrc.o
	$(CC) $(CFLAGS) -DNLS_TRANSLATE_TR -o $@ $(GTK_CFLAGS) $< langs.o gtkrc.o libnls/libnls.a $(GTK_LIBS) $(LDFLAGS)

mxqt-settings: mxqt-settings.tr.cc country.o preferences.xml.h mxqt-settings.moc langs.o qrc.o libnls/libnls.a preferences.xml.tr.c
	$(CXX) $(CXXFLAGS) -DNLS_TRANSLATE_TR -o $@ $(QT_CFLAGS) $< langs.o libnls/libnls.a $(QT_LIBS) $(LDFLAGS)

mxqt-settings.tr.o: mxqt-settings.moc

nlstool: nlstool-nonls

preferences.xml.tr.c: preferences.xml.c nlstool-nonls
	./nlstool-nonls -p $(PO_DIR) --default-domain $(GETTEXT_PACKAGE) translate all $<

preferences.xml.c: $(srcdir)/preferences.xml $(srcdir)/xml-dump.pl
	$(PERL) $(srcdir)/xml-dump.pl $< > $@.$$$$ && { mv $@.$$$$ $@; }

#
# bootstrap: compile a version of nlstool that does not use NLS itself
#
nlstool-nonls: $(NLSTOOL_NONLS_OBJS) libnls/libnls.a
	$(CC) $(CFLAGS) -o $@ $(NLSTOOL_NONLS_OBJS) libnls/libnls.a $(LDFLAGS)

nlstool-nonls.o: nlstool.c nls-enable.h pofile.h libnls/libnls.h $(MAKEFILE_LIST)
	$(CC) $(CFLAGS) -UENABLE_NLS -c -o $@ $<

pofile-nonls.o: pofile.c nls-enable.h pofile.h libnls/libnls.h libnls/libnlsI.h libnls/expreval.h libnls/expreval-internal.h $(MAKEFILE_LIST)
	$(CC) $(CFLAGS) -UENABLE_NLS -c -o $@ $<


#
# each of the *.tr.c files depends on all sources to be translated,
# since we must assign unique msg ids for them
#
define TR_TEMPLATE
$(1).tr$(2): $(1)$(2) $(POTFILES) $(PO_DIR)/$(GETTEXT_PACKAGE).pot nlstool-nonls $(PO_DIR)/LINGUAS
	./nlstool-nonls -p $(PO_DIR) --default-domain $(GETTEXT_PACKAGE) translate all $(1)$(2)

$(1).o: $(1).tr$(2)
	$(if $(filter %.c,$(2)),$(CC) -DNLS_TRANSLATE_TR $(CFLAGS),$(CXX) -DNLS_TRANSLATE_TR $(CXXFLAGS)) -c -o $$@ $$<

endef

$(foreach SRC,$(filter-out gxmessage.c,$(POTFILES)),$(eval $(call TR_TEMPLATE,$(basename $(SRC)),$(suffix $(SRC)))))

# gxmessage still uses libintl
gxmessage.tr.c: gxmessage.c
	cp $< $@

else
gxmessage: gxmessage.c $(MAKEFILE_LIST)
	$(CC) $(CFLAGS) -o $@ $(GTK_CFLAGS) $< $(GTK_LIBS) $(LDFLAGS)

mxgtk-settings: mxgtk-settings.c country.o preferences.xml.h preferences.xml.c gtkrc.o
	$(CC) $(CFLAGS) -o $@ $(GTK_CFLAGS) $< gtkrc.o $(GTK_LIBS) $(LDFLAGS)

mxqt-settings: mxqt-settings.cc country.o preferences.xml.h preferences.xml.c mxqt-settings.moc qrc.o libnls/libnls.a
	$(CXX) $(CXXFLAGS) -o $@ $(QT_CFLAGS) $< $(QT_LIBS) $(LDFLAGS)

mxqt-settings.o: mxqt-settings.moc

endif

mxqt-settings.moc: mxqt-settings.cc
	$(MOC) $(if $(filter yes,$(ENABLE_NLS)),-DENABLE_NLS) -o$@ $<

preferences.xml.h: preferences.xml
	sed -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/"/' $< > $@

# prevent make from trying to compile preferences.xml.c to preferences.xml
$(srcdir)/preferences.xml:
	@:

qrc.o: qrc.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

qrc.cc: icons.qrc $(FLAG_ICONS)
	$(RCC) --namespace -g cpp -o $@ $<


country.o: country.c
langs.o: langs.c

$(NLSTOOL_OBJS): pofile.h nls-enable.h libnls/libnls.h $(MAKEFILE_LIST)

nlstool: $(NLSTOOL_OBJS) libnls/libnls.a
	$(CC) $(CFLAGS) -o $@ $(NLSTOOL_OBJS) libnls/libnls.a $(LDFLAGS)

langs.c: nlstool-nonls $(POTFILES) $(PO_DIR)/$(GETTEXT_PACKAGE).pot
	./nlstool-nonls -p $(PO_DIR) --default-domain $(GETTEXT_PACKAGE) -o $@ make || { $(RM) $@; false; }

libnls/libnls.a: $(LIBNLS_OBJS)
	@$(RM) $@
	ar rcs $@ $(LIBNLS_OBJS)

$(LIBNLS_OBJS): libnls/libnls.h libnls/libnlsI.h libnls/expreval.h libnls/expreval-internal.h $(MAKEFILE_LIST)

expreval-test: libnls/expreval.c libnls/expreval-exp.c libnls/expreval-silent.c libnls/expreval-print.o libnls/plurals.o
	$(CC) $(CFLAGS) -o $@ -DMAIN $< libnls/expreval-exp.c libnls/expreval-print.c libnls/plurals.o $(LDFLAGS)

libnls/expreval-silent.o: libnls/expreval-silent.c libnls/expreval.c

clean::
	$(RM) *.o libnls/*.o libnls/*.a $(PROGRAMS) nlstool-nonls preferences.xml.h preferences.xml.c *.moc qrc.cc libnls/plural.c langs.c *.tr.*
