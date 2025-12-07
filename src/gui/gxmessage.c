/* gxmessage - an xmessage clone using GTK
 *
 * Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009,
 * 2012, 2015 Timothy Richard Musson
 *
 * Merged versions from 2.20.4 & 3.4.3, and ported to GTK 4.x
 *
 * Email: Tim Musson <trmusson@gmail.com>
 * WWW:   http://homepages.ihug.co.nz/~trmusson/programs.html#gxmessage
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define PACKAGE "gxmessage"
#define GETTEXT_PACKAGE "gxmessage"
#define GDK_DISABLE_DEPRECATION_WARNINGS
#define GTK_DISABLE_DEPRECATION_WARNINGS
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#if GTK_CHECK_VERSION(4, 0, 0)
#define VERSION "4.1.1"
#elif GTK_CHECK_VERSION(3, 0, 0)
#define VERSION "3.4.3"
#else
#define VERSION "2.20.4"
#endif

/* Details for Copyright and bug report messages: */
#define AUTHOR  "Timothy Richard Musson"
#define YEAR    "2015"
#define MAILTO  "<trmusson@gmail.com>"

#define MAX_WINDOW_SIZE (G_MAXUINT16 / 2)

#define ENABLE_NLS

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) gettext(String)
#define N_(String) (String)
#else
#define _(String) (String)
#define N_(String) (String)
#define textdomain(Domain)
#define gettext(String) (String)
#define dgettext(Domain,String) (String)
#define dcgettext(Domain,String,Type) (String)
#define bindtextdomain(Domain,Directory)
#endif /* ENABLE_NLS */

#undef g_utf8_next_char
#define g_utf8_next_char(p) ((p) + g_utf8_skip[(unsigned char)(*p)])

typedef struct _Button Button;

#if GTK_CHECK_VERSION(4, 0, 0)
typedef enum
{
  GTK_WIN_POS_NONE,
  GTK_WIN_POS_CENTER,
  GTK_WIN_POS_MOUSE,
  GTK_WIN_POS_CENTER_ALWAYS,
  GTK_WIN_POS_CENTER_ON_PARENT
} GtkWindowPosition;
#endif


struct
{
	gchar *message_text;
	gint message_len;
	Button *button_list;
	const gchar *default_str;
	const gchar *title_str;
	const gchar *geom_str;
	const gchar *font_str;
	const gchar *color_fg;
	const gchar *color_bg;
	const gchar *encoding;
	const gchar *entry_str;
	gint timeout;
	guint timeout_id;
	gboolean do_iconify;
	gboolean do_ontop;
	gboolean do_print;
	gboolean do_buttons;
	gboolean do_borderless;
	gboolean do_sticky;
	gboolean do_focus;
	gboolean allow_escape;
	GtkWrapMode wrap_mode;
	GtkWindowPosition window_position;
	GtkWidget *entry_widget;
	gint exit_code;
} gx;


struct _Button
{
	gboolean is_default;
	gint value;
	const gchar *label;
	Button *prev;
	Button *next;
};


struct Option
{
	gint min_len;						/* support -bu, -but, -butt, etc., as with xmessage */
	gboolean requires_arg;
	gchar *opt_str;
};


static struct Option option[] = {
	{ 2, TRUE, "buttons" },
	{ 1, FALSE, "center" },
	{ 2, TRUE, "default" },
	{ 2, TRUE, "file" },
	{ 2, FALSE, "nearmouse" },
	{ 1, FALSE, "print" },
	{ 3, TRUE, "timeout" },
	{ 2, TRUE, "fn" },
	{ 2, TRUE, "font" },
	{ 1, TRUE, "geometry" },
	{ 3, TRUE, "title" },
	{ 2, TRUE, "bg" },
	{ 2, TRUE, "fg" },
	{ 2, TRUE, "bd" },
	{ 2, TRUE, "bw" },
	{ 1, FALSE, "iconic" },
	{ 1, FALSE, "ontop" },
	{ 2, TRUE, "xrm" },
	{ 2, FALSE, "rv" },
	{ 2, FALSE, "reverse" },
	{ 2, TRUE, "selectionTimeout" },
	{ 2, FALSE, "synchronous" },
	{ 2, TRUE, "xnllanguage" },
	{ 2, TRUE, "name" },
	{ 2, TRUE, "display" },
	{ 2, FALSE, "borderless" },
	{ 2, FALSE, "sticky" },
	{ 1, FALSE, "wrap" },
	{ 3, TRUE, "encoding" },
	{ 3, FALSE, "nofocus" },
	{ 3, FALSE, "noescape" },
	{ 6, TRUE, "entrytext" },
	{ 3, FALSE, "entry" },
	{ 1, FALSE, "?" },
	{ 1, FALSE, "help" },
	{ 1, FALSE, "version" }
};

enum
{
	OPT_IS_UNKNOWN = -2,
	OPT_IS_MISSING_ARG,
	OPT_BUTTONS,
	OPT_CENTER,
	OPT_DEFAULT,
	OPT_FILE,
	OPT_NEARMOUSE,
	OPT_PRINT,
	OPT_TIMEOUT,
	OPT_FN,
	OPT_FONT,
	OPT_GEOMETRY,
	OPT_TITLE,
	OPT_BG,
	OPT_FG,
	OPT_BD,
	OPT_BW,
	OPT_ICONIC,
	OPT_ONTOP,
	OPT_XRM,
	OPT_RV,
	OPT_REVERSE,
	OPT_SELECTIONTIMEOUT,
	OPT_SYNCHRONOUS,
	OPT_XNLLANGUAGE,
	OPT_NAME,
	OPT_DISPLAY,
	OPT_BORDERLESS,
	OPT_STICKY,
	OPT_WRAP,
	OPT_ENCODING,
	OPT_FOCUS,
	OPT_NOESCAPE,
	OPT_ENTRYTEXT,
	OPT_ENTRY,
	OPT_HELP_Q,
	OPT_HELP,
	OPT_VERSION,
	N_OPTS
};


#if GTK_CHECK_VERSION(4, 0, 0)
static GtkWidget *gtk_vbox_new(gboolean homogeneous, int spacing)
{
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, spacing);
	gtk_box_set_homogeneous(GTK_BOX(box), homogeneous);
	return box;
}

static GtkWidget *gtk_hbox_new(gboolean homogeneous, int spacing)
{
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, spacing);
	gtk_box_set_homogeneous(GTK_BOX(box), homogeneous);
	return box;
}

/*** ---------------------------------------------------------------------- ***/

static const char *gtk_entry_get_text(GtkEntry *entry)
{
	GtkEntryBuffer *buffer = gtk_entry_get_buffer(entry);
	return gtk_entry_buffer_get_text(buffer);
}

static void gtk_entry_set_text(GtkEntry *entry, const char *text)
{
	GtkEntryBuffer *buffer = gtk_entry_get_buffer(entry);
	gtk_entry_buffer_set_text(buffer, text, strlen(text));
}

/*** ---------------------------------------------------------------------- ***/

static void gtk_box_pack_start(GtkBox *box, GtkWidget *child, gboolean expand, gboolean fill, guint padding)
{
	gtk_box_prepend(box, child);
	if (expand)
	{
		if (gtk_orientable_get_orientation(GTK_ORIENTABLE(box)) == GTK_ORIENTATION_HORIZONTAL)
			gtk_widget_set_hexpand(child, TRUE);
		else
			gtk_widget_set_vexpand(child, TRUE);
	}
	if (fill)
	{
		if (gtk_orientable_get_orientation(GTK_ORIENTABLE(box)) == GTK_ORIENTATION_HORIZONTAL)
			gtk_widget_set_halign(child, GTK_ALIGN_FILL);
		else
			gtk_widget_set_valign(child, GTK_ALIGN_FILL);
	}
	(void)padding;
}

/*** ---------------------------------------------------------------------- ***/

static void gtk_box_pack_end(GtkBox *box, GtkWidget *child, gboolean expand, gboolean fill, guint padding)
{
	gtk_box_append(box, child);
	if (expand)
	{
		if (gtk_orientable_get_orientation(GTK_ORIENTABLE(box)) == GTK_ORIENTATION_HORIZONTAL)
			gtk_widget_set_hexpand(child, TRUE);
		else
			gtk_widget_set_vexpand(child, TRUE);
	}
	if (fill)
	{
		if (gtk_orientable_get_orientation(GTK_ORIENTABLE(box)) == GTK_ORIENTATION_HORIZONTAL)
			gtk_widget_set_halign(child, GTK_ALIGN_FILL);
		else
			gtk_widget_set_valign(child, GTK_ALIGN_FILL);
	}
	(void)padding;
}

static void gtk_widget_destroy(GtkWidget *w)
{
	gtk_widget_unparent(w);
}

static gint gdk_screen_width(void)
{
	GdkRectangle geom;
	GListModel *monitors = gdk_display_get_monitors(gdk_display_get_default());
	GdkMonitor *monitor = g_list_model_get_item(monitors, 0);
	gdk_monitor_get_geometry(monitor, &geom);
	return geom.width;
}

static gint gdk_screen_height(void)
{
	GdkRectangle geom;
	GListModel *monitors = gdk_display_get_monitors(gdk_display_get_default());
	GdkMonitor *monitor = g_list_model_get_item(monitors, 0);
	gdk_monitor_get_geometry(monitor, &geom);
	return geom.height;
}

#define gtk_hbutton_box_new() gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);

static void gtk_widget_set_border_width(GtkWidget *w, int width)
{
#if 0
	gtk_widget_set_margin_start(w, width);
	gtk_widget_set_margin_end(w, width);
	gtk_widget_set_margin_top(w, width);
	gtk_widget_set_margin_bottom(w, width);
#else
	GtkCssProvider *p;
	GtkStyleContext *style;
	char *str;

	p = gtk_css_provider_new();
	str = g_strdup_printf("#%s { margin: %dpx; }", gtk_widget_get_name(w), width);
	gtk_css_provider_load_from_string(p, str);
	style = gtk_widget_get_style_context(w);
	gtk_style_context_add_provider(style, GTK_STYLE_PROVIDER(p), GTK_STYLE_PROVIDER_PRIORITY_USER);
	g_free(str);
	g_object_unref(G_OBJECT(p));
#endif
}

static void gtk_widget_set_color(GtkWidget *w, const GdkRGBA *color)
{
	GtkCssProvider *p;
	GtkStyleContext *style;
	char *str;

	p = gtk_css_provider_new();
	str = g_strdup_printf("#%s { color: #%02x%02x%02x; }", gtk_widget_get_name(w), (unsigned int)(color->red * 255.0), (unsigned int)(color->green * 255.0), (unsigned int)(color->blue * 255.0));
	gtk_css_provider_load_from_string(p, str);
	style = gtk_widget_get_style_context(w);
	gtk_style_context_add_provider(style, GTK_STYLE_PROVIDER(p), GTK_STYLE_PROVIDER_PRIORITY_USER);
	g_free(str);
	g_object_unref(G_OBJECT(p));
}

static void gtk_widget_set_bgcolor(GtkWidget *w, const GdkRGBA *color)
{
	GtkCssProvider *p;
	GtkStyleContext *style;
	char *str;

	p = gtk_css_provider_new();
	str = g_strdup_printf("#%s { background-color: #%02x%02x%02x; }", gtk_widget_get_name(w), (unsigned int)(color->red * 255.0), (unsigned int)(color->green * 255.0), (unsigned int)(color->blue * 255.0));
	gtk_css_provider_load_from_string(p, str);
	style = gtk_widget_get_style_context(w);
	gtk_style_context_add_provider(style, GTK_STYLE_PROVIDER(p), GTK_STYLE_PROVIDER_PRIORITY_USER);
	g_free(str);
	g_object_unref(G_OBJECT(p));
}

static char *capitalize_string(const char *str)
{
	char first[8] = "";

	if (!str)
		return NULL;

	g_unichar_to_utf8(g_unichar_totitle(g_utf8_get_char(str)), first);
	str = g_utf8_next_char(str);
	return g_strconcat(first, str, NULL);
}

static GtkWidget *gtk_button_new_from_stock(const char *name)
{
	GtkWidget *button;
	GtkWidget *label;
	GtkWidget *icon = gtk_image_new_from_icon_name(name);
	GtkWidget *hbox = gtk_hbox_new(FALSE, 5);

	/*
	 * See https://www.csparks.com/gtk2-html-2.24.33/gtk2-Stock-Items.html
	 * for a list of possible names
	 */
	if (strncmp(name, "gtk-", 4) == 0)
		name += 4;
	if (strcmp(name, "ok") == 0)
		label = gtk_label_new_with_mnemonic(_("_OK"));
	else if (strcmp(name, "cancel") == 0)
		label = gtk_label_new_with_mnemonic(_("_Cancel"));
	else {
		char *str = capitalize_string(name);
		label = gtk_label_new(str);
		g_free(str);
	}
	gtk_box_append(GTK_BOX(hbox), icon);
	gtk_box_append(GTK_BOX(hbox), label);
	gtk_widget_show(icon);
	gtk_widget_show(label);
	gtk_widget_show(hbox);
	button = gtk_button_new();
	gtk_button_set_child(GTK_BUTTON(button), hbox);
	return button;
}

#endif

#if GTK_CHECK_VERSION(3, 0, 0)
#define gtk_widget_size_request(btn, req) gtk_widget_get_preferred_size(btn, req, NULL)
#endif

static Button *button_first(Button *button)
{
	if (button != NULL)
	{
		while (button->prev != NULL)
		{
			button = button->prev;
		}
	}
	return button;
}


static Button *button_append(Button *button, Button *button_new)
{
	if (button != NULL)
	{
		button_new->prev = button;
		button->next = button_new;
	}
	return button_new;
}


static void button_free_all(Button *button)
{
	Button *next;

	button = button_first(button);
	while (button != NULL)
	{
		next = button->next;
		g_free(button);
		button = next;
	}
}


static void prog_cleanup(void)
{
	button_free_all(gx.button_list);
	if (gx.message_text != NULL)
	{
		g_free(gx.message_text);
	}
	if (gx.timeout_id != 0)
	{
		g_source_remove(gx.timeout_id);
	}
	exit(gx.exit_code);
}


static gint parse_label_value_pair(gchar *str, gint value, gint *len, gboolean *end)
{
	gchar *colon = NULL;

	*end = FALSE;
	*len = 0;

	while (*str != '\0')
	{
		if (*str == '\\')
		{
			/* unescape */
			memmove(str, str + 1, strlen(str) + 1);
		} else if (*str == ':')
		{
			/* take note of the last colon found */
			colon = str;
		} else if (*str == ',')
		{
			/* end of pair */
			break;
		}
		str++;
		*len = *len + 1;
	}

	if (*str == '\0')
	{
		*end = TRUE;
	} else
	{
		*str = '\0';
	}

	if (colon)
	{
		/* replace default value with value from string */
		*colon = '\0';
		value = atoi(++colon);
	}

	return value;
}


static Button *button_list_from_str(gchar *str)
{
	/* Split "LABEL:VALUE,LABEL:VALUE,..." into a list of buttons */

	Button *button;
	Button *blist = NULL;
	gint len;
	gint value;
	gint default_value = 101;
	gboolean end;

	if (str == NULL)
		return NULL;

	do
	{
		value = parse_label_value_pair(str, default_value, &len, &end);
		gx.do_buttons |= len > 0;
		button = g_new0(Button, 1);
		button->label = str;
		button->value = value;
		blist = button_append(blist, button);
		str = str + len + 1;
		default_value++;
	} while (!end);

	/* return the last item */
	return blist;
}


static void button_set_default(Button *button, const gchar *str)
{
	/* Make button->is_default TRUE for each button whose label matches str */

	if (str == NULL)
		return;

	button = button_first(button);
	while (button != NULL)
	{
		button->is_default = strcmp(button->label, str) == 0;
		button = button->next;
	}
}


static gint my_get_opt(const gchar *str, gboolean not_last)
{
	/* Try to identify the command line option in str, returning a unique
	 * option/error code. The not_last variable specifies whether current
	 * option was followed by something else (a value or another option)
	 * on the command line.
	 */

	gint opt;
	gint len;

	if (strcmp(str, "+rv") == 0)
		return OPT_RV;
	if (*str != '-')
		return OPT_IS_UNKNOWN;

	str++;
	if (*str == '-')
		str++;
	len = strlen(str);

	if (len > 0)
	{
		for (opt = 0; opt < N_OPTS; opt++)
		{
			if (len >= option[opt].min_len && strncmp(str, option[opt].opt_str, len) == 0)
			{
				if (!option[opt].requires_arg || not_last)
				{
					return opt;
				} else
				{
					return OPT_IS_MISSING_ARG;
				}
			}
		}
	}
	return OPT_IS_UNKNOWN;
}


static void gxmessage_exit(void)
{
#if GTK_CHECK_VERSION(4, 0, 0)
	exit(gx.exit_code);
#else
	gtk_main_quit();
#endif
}

static void cb_window_destroy(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	(void)widget;
	(void)event;
	(void)data;
	gxmessage_exit();
}


#if GTK_CHECK_VERSION(4, 0, 0)
static gboolean cb_key_press(GtkWidget *widget, guint keyval, guint keycode, GdkModifierType state, GtkEventController *event_controller)
{
	(void)widget;
	(void)keycode;
	(void)state;
	(void)event_controller;
	if (gx.allow_escape && keyval == GDK_KEY_Escape)
	{
		gxmessage_exit();
	}
	return FALSE;
}
#else
static gboolean cb_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	(void)widget;
	(void)data;
	if (gx.allow_escape && event->keyval == GDK_KEY_Escape)
	{
		gxmessage_exit();
	}
	return FALSE;
}
#endif


static void cb_button_clicked(GtkWidget *widget, gpointer data)
{
	gx.exit_code = ((Button *) data)->value;

	(void)widget;
	(void)data;
	if (gx.do_print)
	{
		g_print("%s\n", ((Button *) data)->label);
	} else if (gx.entry_str != NULL)
	{
		g_print("%s\n", gtk_entry_get_text(GTK_ENTRY(gx.entry_widget)));
	}
	gxmessage_exit();
}


static void cb_entry_activated(GtkWidget *widget, gpointer data)
{
	(void)widget;
	(void)data;
	gx.exit_code = 0;
	g_print("%s\n", gtk_entry_get_text(GTK_ENTRY(gx.entry_widget)));
	gxmessage_exit();
}


static gboolean cb_timeout(gpointer _timeout)
{
	gint *timeout = (gint *)_timeout;
	static gint counter = 0;

	if (++counter >= *timeout)
	{
		gx.exit_code = 0;
		gxmessage_exit();
	}
	return TRUE;
}


static gboolean label_width_okay(const gchar *str)
{
	GtkWidget *dummy;
	PangoLayout *layout;
	int width;

	dummy = gtk_label_new(NULL);
	layout = gtk_label_get_layout(GTK_LABEL(dummy));
	pango_layout_set_text(layout, str, -1);
	pango_layout_get_pixel_size(layout, &width, NULL);
	gtk_widget_destroy(dummy);

	if (width > MAX_WINDOW_SIZE)
	{
		g_warning("%s: button too wide\n", PACKAGE);
		return FALSE;
	}

	return TRUE;
}


static void window_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GtkWidget *scroller;
	GtkWidget *btn_box;
	GtkWidget *btn = 0;
	GtkWidget *message_widget;
	Button *button;
#if GTK_CHECK_VERSION(3, 0, 0)
	GdkRGBA color;
#else
	GdkColor color;
#endif
	GtkRequisition size_req;
	GtkTextBuffer *buf;
	GtkTextIter iter;
	gint win_w, win_h;
	gint max_w, max_h;


	gtk_window_set_default_icon_name("gxmessage");

#if GTK_CHECK_VERSION(4, 0, 0)
	window = gtk_window_new();
#else
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_accept_focus(GTK_WINDOW(window), gx.do_focus);
#endif
	gtk_widget_set_name(window, "mainwindow");

	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(cb_window_destroy), NULL);

#if GTK_CHECK_VERSION(4, 0, 0)
	{
		GtkEventController *event_controller = gtk_event_controller_key_new();
		g_signal_connect_object(event_controller, "key-pressed", G_CALLBACK(cb_key_press), G_OBJECT(window), G_CONNECT_SWAPPED);
		gtk_widget_add_controller(GTK_WIDGET(window), event_controller);
	}
#else
	g_signal_connect(G_OBJECT(window), "key_press_event", G_CALLBACK(cb_key_press), NULL);
#endif

	if (gx.title_str != NULL)
	{
		gtk_window_set_title(GTK_WINDOW(window), gx.title_str);
	}

	if (gx.do_iconify)
	{
#if GTK_CHECK_VERSION(4, 0, 0)
		gtk_window_minimize(GTK_WINDOW(window));
#else
		gtk_window_iconify(GTK_WINDOW(window));
#endif
	}

	if (gx.do_sticky)
	{
#if !GTK_CHECK_VERSION(4, 0, 0) /* no longer available in GTK 4 */
		gtk_window_stick(GTK_WINDOW(window));
#endif
	}

	if (gx.do_ontop)
	{
#if !GTK_CHECK_VERSION(4, 0, 0) /* no longer available in GTK 4 */
		gtk_window_set_keep_above(GTK_WINDOW(window), TRUE);
#endif
	}

	if (gx.do_borderless)
	{
		gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
	}

	/* window contents */
#if GTK_CHECK_VERSION(4, 0, 0)
	/* gtk_widget_set_border_width(window, 12); */
#else
	gtk_container_set_border_width(GTK_CONTAINER(window), 12);
#endif

	vbox = gtk_vbox_new(FALSE, 12);
	gtk_widget_set_name(vbox, "vbox");
	gtk_widget_show(vbox);
#if GTK_CHECK_VERSION(4, 0, 0)
	gtk_widget_set_border_width(vbox, 12);
	gtk_window_set_child(GTK_WINDOW(window), vbox);
#else
	gtk_container_add(GTK_CONTAINER(window), vbox);
#endif

	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox2);
	gtk_widget_set_name(vbox2, "vbox2");
#if GTK_CHECK_VERSION(4, 0, 0)
	/* gtk_widget_set_border_width(vbox2, 12); */
#else
	/* gtk_container_set_border_width(GTK_CONTAINER(vbox2), 0); not needed */
#endif
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, TRUE, TRUE, 0);

#if GTK_CHECK_VERSION(4, 0, 0)
	scroller = gtk_scrolled_window_new();

	gtk_box_pack_start(GTK_BOX(vbox2), scroller, TRUE, TRUE, 0);
#else
	scroller = gtk_scrolled_window_new(NULL, NULL);
	/* gtk_container_set_border_width(GTK_CONTAINER(scroller), 0); not needed */
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroller), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_placement(GTK_SCROLLED_WINDOW(scroller), GTK_CORNER_TOP_LEFT);
	gtk_box_pack_start(GTK_BOX(vbox2), scroller, TRUE, TRUE, 0);
#endif
	gtk_widget_set_name(scroller, "scroller");
	gtk_widget_show(scroller);

	/* the message */
	message_widget = gtk_text_view_new();
	gtk_widget_set_name(message_widget, "textview");
	gtk_widget_show(message_widget);
	gtk_widget_set_name(message_widget, "gxmessage-textview");
	gtk_text_view_set_editable(GTK_TEXT_VIEW(message_widget), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(message_widget), TRUE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(message_widget), gx.wrap_mode);

	if (gx.font_str != NULL)
	{
		PangoFontDescription *font_desc;

		font_desc = pango_font_description_from_string(gx.font_str);
#if GTK_CHECK_VERSION(4, 0, 0)
		/* TODO */
#else
		gtk_widget_modify_font(message_widget, font_desc);
#endif
		pango_font_description_free(font_desc);
	}

#if GTK_CHECK_VERSION(4, 0, 0)
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), message_widget);
#else
	gtk_container_add(GTK_CONTAINER(scroller), message_widget);
#endif

	buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(message_widget));
	gtk_text_buffer_set_text(buf, gx.message_text, strlen(gx.message_text));

	gtk_text_buffer_get_start_iter(buf, &iter);
	gtk_text_buffer_place_cursor(buf, &iter);

	if (gx.color_fg != NULL)
	{
#if GTK_CHECK_VERSION(3, 0, 0)
		if (gdk_rgba_parse(&color, gx.color_fg))
		{
#if GTK_CHECK_VERSION(4, 0, 0)
			gtk_widget_set_color(message_widget, &color);
#else
			gtk_widget_override_color(message_widget, (GtkStateFlags)GTK_STATE_NORMAL, &color);
#endif
		}
#else
		if (gdk_color_parse(gx.color_fg, &color))
		{
			gtk_widget_modify_text(message_widget, GTK_STATE_NORMAL, &color);
		}
#endif
	}
	if (gx.color_bg != NULL)
	{
#if GTK_CHECK_VERSION(3, 0, 0)
		if (gdk_rgba_parse(&color, gx.color_bg))
		{
#if GTK_CHECK_VERSION(4, 0, 0)
			gtk_widget_set_bgcolor(message_widget, &color);
#else
			gtk_widget_override_background_color(message_widget, (GtkStateFlags)GTK_STATE_NORMAL, &color);
#endif
		}
#else
		if (gdk_color_parse(gx.color_bg, &color))
		{
			gtk_widget_modify_base(message_widget, GTK_STATE_NORMAL, &color);
		}
#endif
	}

	/* text entry */
	if (gx.entry_str != NULL)
	{
		gx.entry_widget = gtk_entry_new();
		gtk_widget_set_name(gx.entry_widget, "entry");
		gtk_widget_show(gx.entry_widget);
		gtk_widget_set_name(gx.entry_widget, "gxmessage-entry");
		gtk_editable_set_editable(GTK_EDITABLE(gx.entry_widget), TRUE);
		gtk_entry_set_text(GTK_ENTRY(gx.entry_widget), gx.entry_str);
		gtk_box_pack_start(GTK_BOX(vbox), gx.entry_widget, FALSE, FALSE, 5);
		gtk_widget_grab_focus(gx.entry_widget);
		if (!gx.do_buttons)
		{
			/* allow hitting <RETURN> to close the window */
			g_signal_connect(G_OBJECT(gx.entry_widget), "activate", G_CALLBACK(cb_entry_activated), (gpointer) 0);
		}
	}


	/* add buttons */
	if (gx.do_buttons)
	{
		button = button_first(gx.button_list);

#if GTK_CHECK_VERSION(4, 0, 0)
		btn_box = gtk_hbox_new(FALSE, 0);
#else
		btn_box = gtk_hbutton_box_new();
		gtk_button_box_set_layout(GTK_BUTTON_BOX(btn_box), GTK_BUTTONBOX_END);
#endif
		gtk_box_set_spacing(GTK_BOX(btn_box), 6);
		gtk_box_pack_end(GTK_BOX(vbox), btn_box, FALSE, FALSE, 0);
		gtk_widget_show(btn_box);

		while (button != NULL)
		{
			if (strcmp(button->label, "okay") == 0)
			{
				btn = gtk_button_new_from_stock("gtk-ok");
			} else if (g_str_has_prefix(button->label, "GTK_STOCK_"))
			{
				gchar *s;
				gchar *p;

				p = g_ascii_strdown(button->label + 10, -1);
				s = p;
				while (*s != '\0')
				{
					if (*s == '_')
						*s = '-';
					s++;
				}
				s = g_strconcat("gtk-", p, NULL);
				if (!label_width_okay(s))
				{
					/* XXX: Truncate and carry on, or quit? */
					prog_cleanup();
				}
				btn = gtk_button_new_from_stock(s);
#if GTK_CHECK_VERSION(4, 0, 0)
				g_free(s);
				s = capitalize_string(p);
				gtk_label_set_text(GTK_LABEL(gtk_widget_get_last_child(gtk_widget_get_first_child(btn))), s);
#endif
				g_free(s);
				g_free(p);
			} else
			{
				if (!label_width_okay(button->label))
				{
					/* XXX: Truncate and carry on, or quit? */
					prog_cleanup();
				}
				btn = gtk_button_new_with_mnemonic(button->label);
			}
			gtk_widget_show(btn);

			g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(cb_button_clicked), (gpointer) button);
			gtk_box_pack_start(GTK_BOX(btn_box), btn, FALSE, FALSE, 0);

			if (button->is_default)
			{
				gtk_widget_grab_focus(btn);
			}

			button = button->next;
		}
	}


	/* window geometry */

	max_w = gdk_screen_width() * 0.7;
	max_h = gdk_screen_height() * 0.7;

	/* Render dummy text, to get an idea of its size. This is slow when
	 * there's a lot of text, so default to max_w and max_h in that case.
	 */
	if (gx.message_len > 20000)
	{
		win_w = max_w;
		win_h = max_h;
	} else
	{
		GtkWidget *dummy = gtk_label_new(gx.message_text);

#if GTK_CHECK_VERSION(3, 0, 0)
		{
#if GTK_CHECK_VERSION(4, 0, 0)
			/* TODO */
#else
			GtkStyleContext *context;
			PangoFontDescription *font_desc;
	
			context = gtk_widget_get_style_context(message_widget);
			gtk_style_context_get(context, GTK_STATE_FLAG_NORMAL, GTK_STYLE_PROPERTY_FONT, &font_desc, NULL);
			gtk_widget_override_font(dummy, font_desc);
			pango_font_description_free(font_desc);
#endif
		}
#else
		gtk_widget_modify_font(dummy, gtk_widget_get_style(message_widget)->font_desc);
#endif
#if GTK_CHECK_VERSION(4, 0, 0)
		gtk_box_append(GTK_BOX(vbox), dummy);
#else
		gtk_container_add(GTK_CONTAINER(vbox), dummy);
#endif
		gtk_widget_show(dummy);
		gtk_widget_size_request(dummy, &size_req);
		gtk_widget_destroy(dummy);
		/* ~50 pixels for borders and scrollbar space */
		win_w = size_req.width + 50;
		win_h = size_req.height + 50;
		if (win_w > max_w)
			win_w = max_w;
		if (win_h > max_h)
			win_h = max_h;
	}

	if (gx.entry_str != NULL)
	{
		gtk_widget_size_request(gx.entry_widget, &size_req);
		win_h += size_req.height + 12;
	}

	if (gx.do_buttons && btn)
	{
		gtk_widget_size_request(btn, &size_req);
		win_h += size_req.height + 12;
	}

#if !GTK_CHECK_VERSION(4, 0, 0)
	gtk_window_set_position(GTK_WINDOW(window), gx.window_position);
#endif
	gtk_window_set_default_size(GTK_WINDOW(window), win_w, win_h);

	if (gx.geom_str != NULL)
	{
		gtk_widget_show(vbox);		/* must precede parse_geometry */
#if !GTK_CHECK_VERSION(4, 0, 0) /* no longer available in GTK 4 */
		gtk_window_parse_geometry(GTK_WINDOW(window), gx.geom_str);
#endif
	}

	/* open the window */
	gtk_widget_show(window);

	/* begin timeout */
	if (gx.timeout != 0)
	{
		gx.timeout_id = g_timeout_add(1000, cb_timeout, &gx.timeout);
	}
}


static gchar *read_stdin(void)
{
	GString *text;
	gchar *str;
	gint ch;

	text = g_string_new("");

	while ((ch = getc(stdin)) != EOF)
	{
		g_string_append_c(text, ch);
	}
	str = g_string_free(text, FALSE);
	return str;
}


static gchar *str_to_utf8(const gchar *str)
{
	gchar *result;
	GError *error = NULL;

	if (gx.encoding == NULL)
	{
		/* assume message encoding matches current locale */
		result = g_locale_to_utf8(str, -1, NULL, NULL, NULL);
	} else
	{
		/* use encoding specified on command line */
		result = g_convert_with_fallback(str, -1, "UTF-8", gx.encoding, NULL, NULL, NULL, NULL);
	}

	if (result == NULL)
	{
		/* fall back to ISO-8859-1 as source encoding */
		result = g_convert_with_fallback(str, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL, &error);
		if (result == NULL)
		{
			if (error != NULL && error->message != NULL)
			{
				g_printerr(PACKAGE ": %s\n", error->message);
			}
			prog_cleanup();
		}
	}
	return result;
}


static gboolean my_gtk_init(gint argc, gchar *argv[])
{
	/* Let gtk_init see --display and --name, but no other options.
	 * Return FALSE if gtk_init fails.
	 */

	gboolean ok;
	gchar *s;
	gchar **av;
	gint i;
	gint len;
	gint n = 1;

	av = g_malloc(sizeof(char *) * (argc + 1));
	av[0] = argv[0];

	for (i = 1; i < argc; i++)
	{
		s = argv[i];
		if (s[0] != '-')
			continue;
		if (s[1] == '-')
			s++;
		len = strlen(s);
		if (len > 2 && i + 1 < argc)
		{
			if (strncmp("-display", s, len) == 0)
			{
				av[n++] = "--display";
				av[n++] = argv[++i];
			} else if (strncmp("-name", s, len) == 0)
			{
				av[n++] = "--name";
				av[n++] = argv[++i];
			}
		}
	}
	av[n] = NULL;
#if GTK_CHECK_VERSION(4, 0, 0)
	gtk_init();
	ok = TRUE;
#else
	ok = gtk_init_check(&n, &av);
#endif
	g_free(av);
	return ok;
}


static void usage(void)
{
	g_print(_("\n%s - a GTK-based xmessage clone\n"), PACKAGE);
	g_print("\n");
	g_print(_("Usage: %s [OPTIONS] message ...\n"), PACKAGE);
	g_print(_("       %s [OPTIONS] -file FILENAME\n"), PACKAGE);
	g_print("\n");
	g_print(_("xmessage options:\n"));
	g_print(_("  -file FILENAME         Get message text from file, '-' for stdin\n"));
	g_print(_("  -buttons BUTTON_LIST   List of \"LABEL:EXIT_CODE\", comma separated\n"));
	g_print(_("  -default LABEL         Give keyboard focus to the specified button\n"));
	g_print(_("  -print                 Send the selected button's LABEL to stdout\n"));
	g_print(_("  -center                Open the window in the center of the screen\n"));
	g_print(_("  -nearmouse             Open the window near the mouse pointer\n"));
	g_print(_("  -timeout SECONDS       Exit with code 0 after SECONDS seconds\n"));
	g_print(_("  -display DISPLAY       X display to use\n"));
	g_print(_("  -fn FONT | -font FONT  Set message font (works with GTK font names)\n"));
	g_print(_("  -fg COLOR              Set message font color\n"));
	g_print(_("  -bg COLOR              Set message background color\n"));
#if !GTK_CHECK_VERSION(4, 0, 0) /* no longer available in GTK 4 */
	g_print(_("  -geometry GEOMETRY     Set window size (position will be ignored)\n"));
#endif
	g_print(_("  -iconic                Start iconified\n"));
	g_print(_("  -name NAME             Program name as used by the window manager\n"));
	g_print(_("  -title TITLE           Set window title to TITLE\n"));
	g_print("\n");
	g_print(_("Additional %s options:\n"), PACKAGE);
	g_print(_("  -borderless            Open the window without border decoration\n"));
#if !GTK_CHECK_VERSION(4, 0, 0) /* no longer available in GTK 4 */
	g_print(_("  -sticky                Make the window stick to all desktops\n"));
	g_print(_("  -ontop                 Keep window on top\n"));
#endif
	g_print(_("  -nofocus               Don't focus the window when it opens\n"));
	g_print(_("  -noescape              Don't allow pressing ESC to close the window\n"));
	g_print(_("  -encoding CHARSET      Expect CHARSET as the message encoding\n"));
	g_print(_("  -entry                 Prompt for text to be sent to stdout\n"));
	g_print(_("  -entrytext TEXT        Same as -entry, but with TEXT as default text\n"));
	g_print(_("  -wrap                  Wrap lines of text to fit window width\n"));
	g_print(_("  -help | -?             Show this usage information\n"));
	g_print(_("  -version               Show gxmessage version and Copyright details\n"));
	g_print("\n");
	g_print(_("Please report bugs to %s.\n"), MAILTO);
	g_print("\n");

	prog_cleanup();
}


int main(gint argc, gchar *argv[])
{
	GString *gstr = NULL;
	gchar *ch = NULL;
	gchar *tmpstr;
	const gchar *fname = NULL;
	gint opt;
	gint arg = 1;
	gboolean ok;
	static gchar bu_default[] = "okay:0";

	/* The default "okay:0" string is intentionally hard-wired, to avoid
	 * breaking scripts that make use of xmessage's -print option.
	 * It must not be changed or gettextize'd. */

#ifdef ENABLE_NLS
#ifndef PACKAGE_LOCALE_DIR
#define PACKAGE_LOCALE_DIR "/usr/share/locale"
#endif
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif

	ok = my_gtk_init(argc, argv);

	gx.exit_code = 1;
	gx.do_ontop = FALSE;
	gx.do_focus = TRUE;
	gx.allow_escape = TRUE;
	gx.wrap_mode = GTK_WRAP_NONE;
	gx.window_position = GTK_WIN_POS_NONE;

	while (arg < argc)
	{
		opt = my_get_opt(argv[arg], arg + 1 < argc);
		switch (opt)
		{
		case OPT_HELP:
		case OPT_HELP_Q:
			gx.exit_code = 0;
			usage();
			return 0;
		case OPT_VERSION:
			g_print(PACKAGE "-" VERSION "\n");
			g_print(_("Copyright (\xC2\xA9) %s %s\n"
					 "This is free software. You may redistribute copies of it under\n"
					 "the terms of the GNU General Public License <https://www.gnu.org/licenses/gpl.html>.\n"
					 "There is NO WARRANTY, to the extent permitted by law.\n"), YEAR, AUTHOR);
			return 0;
		case OPT_ENTRY:
		case OPT_ENTRYTEXT:
			if (gx.do_print)
			{
				g_printerr(_("%s: can't have both -entry and -print\n"), PACKAGE);
				prog_cleanup();
			}
			if (gx.timeout)
			{
				/* -entry disables -timeout */
				gx.timeout = 0;
			}
			if (opt == OPT_ENTRY)
			{
				gx.entry_str = "";
			} else
			{
				gx.entry_str = argv[++arg];
			}
			break;
		case OPT_BUTTONS:
			button_free_all(gx.button_list);
			gx.button_list = button_list_from_str(argv[++arg]);
			break;
		case OPT_CENTER:
			gx.window_position = GTK_WIN_POS_CENTER;
			break;
		case OPT_DEFAULT:
			gx.default_str = argv[++arg];
			break;
		case OPT_FILE:
			if (gstr != NULL)
			{
				g_printerr(_("%s: can't get message from both -file and command line\n"), PACKAGE);
				prog_cleanup();
			}
			fname = argv[++arg];
			break;
		case OPT_NEARMOUSE:
			/* -center takes priority over -nearmouse */
			if (gx.window_position != GTK_WIN_POS_CENTER)
			{
				gx.window_position = GTK_WIN_POS_MOUSE;
			}
			break;
		case OPT_PRINT:
			if (gx.entry_str != NULL)
			{
				g_printerr(_("%s: can't have both -entry and -print\n"), PACKAGE);
				prog_cleanup();
			}
			gx.do_print = TRUE;
			break;
		case OPT_TIMEOUT:
			gx.timeout = strtol(argv[++arg], &ch, 10);
			if (*ch)
			{
				g_printerr(_("%s: integer -timeout value expected\n"), PACKAGE);
				/* continue anyway */
			}
			if (gx.timeout < 0 || gx.entry_str != NULL)
			{
				/* -entry disables -timeout */
				gx.timeout = 0;
			}
			break;
		case OPT_TITLE:
			gx.title_str = argv[++arg];
			break;
		case OPT_GEOMETRY:
			gx.geom_str = argv[++arg];
			break;
		case OPT_FN:
		case OPT_FONT:
			gx.font_str = argv[++arg];
			break;
		case OPT_RV:
		case OPT_REVERSE:
		case OPT_SYNCHRONOUS:
			/* not implemented - ignore */
			break;
		case OPT_BG:
			gx.color_bg = argv[++arg];
			break;
		case OPT_FG:
			gx.color_fg = argv[++arg];
			break;
		case OPT_NAME:
		case OPT_DISPLAY:
			/* already handled by my_gtk_init - ignore and skip arg */
		case OPT_BD:
		case OPT_BW:
		case OPT_XRM:
		case OPT_SELECTIONTIMEOUT:
		case OPT_XNLLANGUAGE:
			/* not implemented - ignore and skip arg */
			arg++;
			break;
		case OPT_ICONIC:
			gx.do_iconify = TRUE;
			break;
		case OPT_ONTOP:
			gx.do_ontop = TRUE;
			break;
		case OPT_STICKY:
			gx.do_sticky = TRUE;
			break;
		case OPT_BORDERLESS:
			gx.do_borderless = TRUE;
			break;
		case OPT_WRAP:
			gx.wrap_mode = GTK_WRAP_WORD;
			break;
		case OPT_ENCODING:
			gx.encoding = argv[++arg];
			break;
		case OPT_FOCUS:
			gx.do_focus = FALSE;
			break;
		case OPT_NOESCAPE:
			gx.allow_escape = FALSE;
			break;
		case OPT_IS_MISSING_ARG:
			/* in this case, xmessage treats the "option" as normal text */
		case OPT_IS_UNKNOWN:
		default:
			if (fname != NULL)
			{
				g_printerr(_("%s: can't get message from both -file and command line\n"), PACKAGE);
				prog_cleanup();
			}
			if (gstr == NULL)
			{
				gstr = g_string_new("");
			} else
			{
				gstr = g_string_append_c(gstr, ' ');
			}
			gstr = g_string_append(gstr, argv[arg]);
			break;
		}
		arg++;
	}

	if (!ok)
	{
		g_printerr("%s: unable to initialize GTK\n", PACKAGE);
		prog_cleanup();
	}

	if (fname != NULL)
	{
		if (strcmp("-", fname) == 0)
		{
			tmpstr = read_stdin();
		} else if (!g_file_get_contents(fname, &tmpstr, NULL, NULL))
		{
			g_printerr(_("%s: unable to read file\n"), PACKAGE);
			prog_cleanup();
		}
		gx.message_text = str_to_utf8(tmpstr);
		gx.message_len = strlen(tmpstr);
		g_free(tmpstr);
	} else if (gstr != NULL)
	{
		gx.message_text = str_to_utf8(gstr->str);
		gx.message_len = gstr->len;
		g_string_free(gstr, TRUE);
	} else
	{
		g_printerr(_("%s: message text is required\n"), PACKAGE);
		g_printerr(_("Try `%s --help' for more information\n"), PACKAGE);
		prog_cleanup();
	}

	if (gx.button_list == NULL)
	{
		gx.button_list = button_list_from_str(bu_default);
	}

	button_set_default(gx.button_list, gx.default_str);

	window_create();
#if GTK_CHECK_VERSION(4, 0, 0)
	while (g_list_model_get_n_items(gtk_window_get_toplevels()) > 0)
		g_main_context_iteration(NULL, TRUE);
#else
	gtk_main();
#endif

	prog_cleanup();
	return 0;
}
