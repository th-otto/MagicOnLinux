# Makefile for program source directory in GNU NLS utilities package.
# Copyright (C) 1995, 1996, 1997 by Ulrich Drepper <drepper@gnu.ai.mit.edu>
# Copyright (C) 2004-2008 Rodney Dawes <dobey.pwns@gmail.com>
#
# This file may be copied and used freely without restrictions.  It may
# be used in projects which are not available under a GNU Public License,
# but which still want to provide support for the GNU gettext functionality.
#
# - Modified by Owen Taylor <otaylor@redhat.com> to use GETTEXT_PACKAGE
#   instead of PACKAGE and to look for po2tbl in ./ not in intl/
#
# - Modified by jacob berkman <jacob@ximian.com> to install
#   Makefile.in.in and po2tbl.sed.in for use with glib-gettextize
#
# - Modified by Rodney Dawes <dobey.pwns@gmail.com> for use with intltool
#
# - Modified by Thorsten Otto for use without intltool
#

GETTEXT_PACKAGE = MagicOnLinux
PACKAGE = $(PACKAGE)
VERSION = 0.3
MSGID_BUGS_ADDRESS = https://github.com/th-otto/MagicOnLinux/issues
USE_NLS = yes

SHELL = /bin/sh
PERL = perl

srcdir = .
top_srcdir = ..
top_builddir = ..
subdir = po

prefix = /usr
exec_prefix = $(prefix)
datadir = $(prefix)/share
datarootdir = $(datadir)
localedir = $(datadir)/locale
subdir = po
mkdir_p = mkdir -p

INSTALL = install -c
INSTALL_DATA = ${INSTALL} -m 644

GMSGFMT = msgfmt
MSGFMT = msgfmt
XGETTEXT = xgettext
MSGMERGE = msgmerge --no-fuzzy-matching --width=100

PO_LINGUAS=$(shell grep -v "^#" $(srcdir)/LINGUAS)

USER_LINGUAS=$(shell if test -n "$(LINGUAS)"; then LLINGUAS="$(LINGUAS)"; ALINGUAS="$(PO_LINGUAS)"; for lang in $$LLINGUAS; do if test -n "`grep '^$$lang$$' $(srcdir)/LINGUAS 2>/dev/null`" -o -n "`echo $$ALINGUAS|tr ' ' '\n'|grep '^$$lang$$'`"; then printf "$$lang "; fi; done; fi)

USE_LINGUAS=$(shell if test -n "$(USER_LINGUAS)" -o -n "$(LINGUAS)"; then LLINGUAS="$(USER_LINGUAS)"; else if test -n "$(PO_LINGUAS)"; then LLINGUAS="$(PO_LINGUAS)"; else LLINGUAS="$(PO_LINGUAS)"; fi; fi; for lang in $$LLINGUAS; do printf "$$lang "; done)

POFILES=$(addsuffix .po,$(PO_LINGUAS))

DISTFILES = Makefile POTFILES $(POFILES)
EXTRA_DISTFILES = LINGUAS

POTFILES = $(shell grep -v "^#" $(srcdir)/POTFILES) $(top_srcdir)/src/gui/preferences.xml

CATALOGS=$(addsuffix .gmo,$(USE_LINGUAS))

XGETTEXT_KEYWORDS = \
	--keyword=_ \
	--keyword=N_ \
	--keyword=U_ \
	--keyword=Q_ \
	--keyword=P_:1,2 \
	--keyword=C_:1c,2 \
	--keyword=NC_:1c,2

XGETTEXT_ARGS = \
	--msgid-bugs-address="$(MSGID_BUGS_ADDRESS)" \
	--default-domain=$(GETTEXT_PACKAGE) \
	--package-name=$(GETTEXT_PACKAGE) \
	--package-version=$(VERSION) \
	--add-comments \
	--flag=g_strdup_printf:1:c-format \
	--flag=g_string_printf:2:c-format \
	--flag=g_string_append_printf:2:c-format \
	--flag=g_error_new:3:c-format \
	--flag=g_set_error:4:c-format \
	--flag=g_markup_printf_escaped:1:c-format \
	--flag=g_log:3:c-format \
	--flag=g_print:1:c-format \
	--flag=g_printerr:1:c-format \
	--flag=g_printf:1:c-format \
	--flag=g_fprintf:2:c-format \
	--flag=g_sprintf:2:c-format \
	--flag=g_snprintf:3:c-format \
	--flag=g_scanner_error:2:c-format \
	--flag=g_scanner_warn:2:c-format \
	--directory=$(srcdir) \
	--files-from=$(srcdir)/POTFILES \
	--no-wrap \
	--width=4 \
	--from-code=UTF-8

.SUFFIXES:
.SUFFIXES: .po .gmo .mo

.po.mo:
	$(MSGFMT) -o $@ $<

.po.gmo:
	@file=$<; file=$${file%.po}; file=$${file##*/}.gmo; $(RM) "$$file"; echo $(GMSGFMT) $$file; $(GMSGFMT) -c -v --statistics -o $$file $<


all: all-$(USE_NLS)

all-yes: $(CATALOGS)
all-no:

$(srcdir)/$(GETTEXT_PACKAGE).pot: $(srcdir)/$(POTFILES) $(srcdir)/Makefile
	@echo generating $@; \
	$(XGETTEXT) $(XGETTEXT_ARGS) $(XGETTEXT_KEYWORDS) --output=$@.tmp; \
	$(PERL) $(srcdir)/xml-extract.pl $(top_srcdir)/src/gui/preferences.xml >> $@.tmp; \
	mv $@.tmp $@

install: install-data
install-data: install-data-$(USE_NLS)
install-data-no: all
install-data-yes: all
	linguas="$(USE_LINGUAS)"; \
	for lang in $$linguas; do \
	  dir=$(DESTDIR)$(localedir)/$$lang/LC_MESSAGES; \
	  $(mkdir_p) $$dir; \
	  if test -r $$lang.gmo; then \
	    $(INSTALL_DATA) $$lang.gmo $$dir/$(GETTEXT_PACKAGE).mo; \
	    echo "installing $$lang.gmo as $$dir/$(GETTEXT_PACKAGE).mo"; \
	  else \
	    $(INSTALL_DATA) $(srcdir)/$$lang.gmo $$dir/$(GETTEXT_PACKAGE).mo; \
	    echo "installing $(srcdir)/$$lang.gmo as" \
		 "$$dir/$(GETTEXT_PACKAGE).mo"; \
	  fi; \
	done

# Empty stubs to satisfy archaic automake needs
dvi info tags TAGS ID:

# Define this as empty until I found a useful application.
install-exec installcheck:

uninstall:
	linguas="$(PO_LINGUAS)"; \
	for lang in $$linguas; do \
	  rm -f $(DESTDIR)$(localedir)/$$lang/LC_MESSAGES/$(GETTEXT_PACKAGE).mo; \
	  rm -f $(DESTDIR)$(localedir)/$$lang/LC_MESSAGES/$(GETTEXT_PACKAGE).mo.m; \
	done

check: all $(srcdir)/$(GETTEXT_PACKAGE).pot

mostlyclean:
	$(RM) $(GETTEXT_PACKAGE).po *.old.po cat-id-tbl.tmp

clean: mostlyclean

distclean: clean
	$(RM) *.mo *.gmo

maintainer-clean: distclean
	@echo "This command is intended for maintainers to use;"
	@echo "it deletes files that may require special tools to rebuild."
	$(RM) Makefile.in.in

distdir = ../$(PACKAGE)-$(VERSION)/$(subdir)
dist distdir: $(DISTFILES)
	dists="$(DISTFILES)"; \
	extra_dists="$(EXTRA_DISTFILES)"; \
	for file in $$extra_dists; do \
	  test -f $(srcdir)/$$file && dists="$$dists $(srcdir)/$$file"; \
	done; \
	for file in $$dists; do \
	  test -f $$file || file="$(srcdir)/$$file"; \
	  ln $$file $(distdir) 2> /dev/null \
	    || cp -p $$file $(distdir); \
	done

update-po: $(srcdir)/Makefile $(srcdir)/$(GETTEXT_PACKAGE).pot
	@tmpdir=`pwd`; \
	linguas="$(USE_LINGUAS)"; \
	for lang in $$linguas; do \
	  printf "$$lang: "; \
	  result="`$(MSGMERGE) -o $$tmpdir/$$lang.new.po $$lang $(srcdir)/$(GETTEXT_PACKAGE).pot`"; \
	  if $$result; then \
	    if cmp $(srcdir)/$$lang.po $$tmpdir/$$lang.new.po >/dev/null 2>&1; then \
	      rm -f $$tmpdir/$$lang.new.po; \
            else \
	      if mv -f $$tmpdir/$$lang.new.po $$lang.po; then \
	        :; \
	      else \
	        echo "msgmerge for $$lang.po failed: cannot move $$tmpdir/$$lang.new.po to $$lang.po" 1>&2; \
	        rm -f $$tmpdir/$$lang.new.po; \
	        exit 1; \
	      fi; \
	    fi; \
	  else \
	    echo "msgmerge for $$lang.gmo failed!"; \
	    rm -f $$tmpdir/$$lang.new.po; \
	  fi; \
	done

# Generates po/POTFILES by examining the source tree:
# that way we don't have to keep this list up to date as files are added.
# FIXME: the generated file list is currently incomplete, since XML files are processed separately
#
generate_potfiles_in:
	@tmp=po.$$$$ ;				\
	 file=$(srcdir)/POTFILES;			\
	 ( cd $(srcdir);			\
	   ( grep -l '_("' ../src/*.cpp ;	\
	     grep -l '_("' ../inc/*.h ;	\
	     grep -l '_("' ../src/gui/*.[ch] ;	\
	     grep -l '_("' ../src/gui/*.cc 	\
	     ) | sort	\
	  ) > $$tmp &&				\
	 mv $$tmp $$file &&			\
	 echo "created `pwd`/$$file" || $(RM) $tmp

# Tell versions [3.59,3.63) of GNU make not to export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
