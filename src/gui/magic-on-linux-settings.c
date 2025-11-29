#define GDK_DISABLE_DEPRECATION_WARNINGS
#define GTK_DISABLE_DEPRECATION_WARNINGS
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

/* no translations currently */
#define _(x) x
#define N_(x) x

#define EXIT_WINDOW_CLOSED (EXIT_FAILURE + EXIT_SUCCESS + 1)

static char const program_name[] = "magic-on-linux-settings";
static int debug_parser = 0;

enum {
	TYPE_NONE,
	TYPE_PATH,
	TYPE_FOLDER,
	TYPE_STRING,
	TYPE_INT,
	TYPE_BOOL,
	TYPE_CHOICE
};

typedef struct {
	GtkWidget *window;
	GtkWidget *notebook;
	
	const char *section_name;
	GtkWidget *section_table;
	int section_row;
	GtkWidget *combo_box;

	GSList *widget_list;
	
	int exit_code;
} GuiWindow;


struct pref_val {
	int type;
	union {
		char *s;
		struct {
			char *p;
			int flags;
		} p;
		long l;
		struct {
			int v;
			int minval;
			int maxval;
		} i;
		double d;
		int b;
		int c;
	} u;
};


static int bool_from_string(const char *str)
{
	if (str == NULL || *str == '\0')
		return FALSE;
	if (g_ascii_strcasecmp(str, "YES") == 0 ||
		g_ascii_strcasecmp(str, "ON") == 0 ||
		g_ascii_strcasecmp(str, "TRUE") == 0 ||
		strcmp(str, "1") == 0)
		return TRUE;
	return FALSE;
}


static void cb_file_changed(GtkWidget *widget, gpointer data)
{
	struct pref_val *val;
	char *path;
	
	(void)data;
	val = g_object_get_data(G_OBJECT(widget), "preference");
	path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));
	g_free(val->u.s);
	val->u.s = path;
}


static char *path_expand(const char *value)
{
	if (*value == '~')
	{
		return g_strconcat(g_get_home_dir(), value + 1, NULL);
	} else
	{
		return g_strdup(value);
	}
}


static void set_value(GtkWidget *w, int type, const char *value, int flags)
{
	struct pref_val val;
	
	memset(&val, 0, sizeof(val));
	val.type = type;
	switch (type)
	{
	case TYPE_STRING:
		if (value)
		{
			gtk_entry_set_text(GTK_ENTRY(w), value);
			val.u.s = g_strdup(value);
		}
		break;
	case TYPE_BOOL:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), bool_from_string(value));
		break;
	case TYPE_PATH:
		if (value)
		{
			/* FIXME: filechooser does not select anything if file does not exist */
			val.u.p.p = path_expand(value);
			gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(w), val.u.p.p);
		}
		val.u.p.flags = flags;
		break;
	case TYPE_FOLDER:
		if (value)
		{
			val.u.p.p = path_expand(value);
			gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(w), val.u.p.p);
		}
		val.u.p.flags = flags;
		break;
	case TYPE_INT:
		if (value)
		{
			val.u.i.v = strtol(value, NULL, 0);
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), val.u.i.v);
		}
		val.u.i.minval = gtk_adjustment_get_lower(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(w)));
		val.u.i.maxval = gtk_adjustment_get_lower(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(w)));
		break;
	case TYPE_CHOICE:
		if (value)
			val.u.c = strtol(value, NULL, 0);
		/* cannot select value until combobox is created */
		break;
	}
	g_object_set_data(G_OBJECT(w), "preference", g_memdup(&val, sizeof(val)));
}


/*
 * Called for opening tags like <foo bar="baz">
 */
static void parser_start_element(GMarkupParseContext *context, const char *element_name, const char **attribute_names, const char **attribute_values, gpointer user_data, GError **error)
{
	GuiWindow *gui = (GuiWindow *)user_data;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *vbox;
	GtkWidget *button;
	gsize i;
	const char *name = 0;
	const char *display = 0;
	const char *default_value = 0;
	GtkWidget *settings_widget = 0;
	int type = TYPE_NONE;
	int flags = 0;

	(void)context;
	(void)error;

	for (i = 0; attribute_names[i]; i++)
	{
		if (debug_parser)
			fprintf(stderr, "attribute: %s = \"%s\"\n", attribute_names[i], attribute_values[i]);
		if (strcmp(attribute_names[i], "name") == 0)
			name = attribute_values[i];
		if (strcmp(attribute_names[i], "display") == 0)
			display = attribute_values[i];
		if (strcmp(attribute_names[i], "default") == 0)
			default_value = attribute_values[i];
	}
	
	if (name == NULL)
	{
		fprintf(stderr, "%s missing name\n", element_name);
	}
	if (display == NULL)
	{
		if (name != NULL)
		{
			char *str;

			str = g_strdup(name);
			display = str; /* FIXME: leaked */
		}
	}

	if (strcmp(element_name, "preferences") == 0)
	{
		; /* ignore */
	} else if (strcmp(element_name, "section") == 0)
	{
		assert(gui->section_table == NULL);
		label = gtk_label_new(N_(display));
		vbox = gtk_vbox_new(FALSE, 0);
		gui->section_name = name;
		gui->section_table = gtk_table_new(2, 2, FALSE);
		gui->section_row = 0;
		gtk_table_set_row_spacings(GTK_TABLE(gui->section_table), 4);
		gtk_table_set_col_spacings(GTK_TABLE(gui->section_table), 4);
		gtk_box_pack_start(GTK_BOX(vbox), gui->section_table, FALSE, FALSE, 0);
		gtk_notebook_append_page(GTK_NOTEBOOK(gui->notebook), vbox, label);
	} else if (strcmp(element_name, "folder") == 0 ||
		strcmp(element_name, "path") == 0)
	{
		assert(gui->section_table != NULL);
		label = gtk_label_new(N_(display));
		type = strcmp(element_name, "folder") == 0 ? TYPE_FOLDER : TYPE_PATH;
		for (i = 0; attribute_names[i]; i++)
		{
			if (strcmp(attribute_names[i], "flags") == 0)
				flags = strtol(attribute_values[i], NULL, 0);
		}
		gtk_table_attach_defaults(GTK_TABLE(gui->section_table), label, 0, 1, gui->section_row, gui->section_row + 1);
		if (type == TYPE_FOLDER)
			button = gtk_file_chooser_button_new(_("Select a Folder"), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
		else
			button = gtk_file_chooser_button_new(_("Select a File"), GTK_FILE_CHOOSER_ACTION_OPEN);
		gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(button), TRUE);
		gtk_table_attach_defaults(GTK_TABLE(gui->section_table), button, 1, 2, gui->section_row, gui->section_row + 1);
		gui->section_row++;
		settings_widget = button;
		g_signal_connect(G_OBJECT(settings_widget), "file-set", G_CALLBACK(cb_file_changed), gui);
		if (strcmp(name, "atari_drv_c") == 0 ||
			strcmp(name, "atari_drv_h") == 0 ||
			strcmp(name, "atari_drv_m") == 0)
			gtk_widget_set_sensitive(button, FALSE);
	} else if (strcmp(element_name, "string") == 0)
	{
		assert(gui->section_table != NULL);
		label = gtk_label_new(N_(display));
		gtk_table_attach_defaults(GTK_TABLE(gui->section_table), label, 0, 1, gui->section_row, gui->section_row + 1);
		entry = gtk_entry_new();
		gtk_table_attach_defaults(GTK_TABLE(gui->section_table), entry, 1, 2, gui->section_row, gui->section_row + 1);
		gui->section_row++;
		settings_widget = entry;
		type = TYPE_STRING;
	} else if (strcmp(element_name, "int") == 0)
	{
		long minval = 0;
		long maxval = LONG_MAX;
		long step = 1;
		GtkObject *adjustment;
		
		assert(gui->section_table != NULL);
		label = gtk_label_new(N_(display));
		for (i = 0; attribute_names[i]; i++)
		{
			if (strcmp(attribute_names[i], "minval") == 0)
				minval = strtol(attribute_values[i], NULL, 0);
			if (strcmp(attribute_names[i], "maxval") == 0)
				maxval = strtol(attribute_values[i], NULL, 0);
			if (strcmp(attribute_names[i], "step") == 0)
				step = strtol(attribute_values[i], NULL, 0);
		}
		adjustment = gtk_adjustment_new(0, minval, maxval, step, step * 16, 0);
		gtk_table_attach_defaults(GTK_TABLE(gui->section_table), label, 0, 1, gui->section_row, gui->section_row + 1);
		button = gtk_spin_button_new(GTK_ADJUSTMENT(adjustment), 0, 0);
		gtk_table_attach_defaults(GTK_TABLE(gui->section_table), button, 1, 2, gui->section_row, gui->section_row + 1);
		gui->section_row++;
		settings_widget = button;
		type = TYPE_INT;
	} else if (strcmp(element_name, "bool") == 0)
	{
		assert(gui->section_table != NULL);
		button = gtk_check_button_new_with_label(N_(display));
		gtk_table_attach_defaults(GTK_TABLE(gui->section_table), button, 1, 2, gui->section_row, gui->section_row + 1);
		gui->section_row++;
		settings_widget = button;
		type = TYPE_BOOL;
	} else if (strcmp(element_name, "choice") == 0)
	{
		assert(gui->section_table != NULL);
		assert(gui->combo_box == NULL);
		label = gtk_label_new(N_(display));
		gtk_table_attach_defaults(GTK_TABLE(gui->section_table), label, 0, 1, gui->section_row, gui->section_row + 1);
		gui->combo_box = gtk_combo_box_new_text();
		gtk_table_attach_defaults(GTK_TABLE(gui->section_table), gui->combo_box, 1, 2, gui->section_row, gui->section_row + 1);
		gui->section_row++;
		settings_widget = gui->combo_box;
		type = TYPE_CHOICE;
	} else if (strcmp(element_name, "select") == 0)
	{
		assert(gui->section_table != NULL);
		assert(gui->combo_box != NULL);
		gtk_combo_box_append_text(GTK_COMBO_BOX(gui->combo_box), N_(display));
	} else
	{
		fprintf(stderr, "unsupported element %s\n", element_name);
	}
	
	if (settings_widget)
	{
		g_object_set_data(G_OBJECT(settings_widget), "section", g_strdup(gui->section_name));
		gtk_widget_set_name(settings_widget, name);
		set_value(settings_widget, type, default_value, flags);
		gui->widget_list = g_slist_append(gui->widget_list, settings_widget);
	}
}


/*
 * Called for closing tags like </foo>
 */
static void parser_end_element(GMarkupParseContext *context, const char *element_name, gpointer user_data, GError **error)
{
	GuiWindow *gui = (GuiWindow *)user_data;

	(void)context;
	(void)error;
	if (strcmp(element_name, "preferences") == 0)
	{
		; /* ignore */
	} else if (strcmp(element_name, "section") == 0)
	{
		assert(gui->section_table != NULL);
		gui->section_table = NULL;
		gui->section_name = NULL;
	} else if (strcmp(element_name, "choice") == 0)
	{
		assert(gui->combo_box != NULL);
		gtk_combo_box_set_active(GTK_COMBO_BOX(gui->combo_box), 0);
		gui->combo_box = NULL;
	}
}


/*
 * Called for character data. Text is nul-terminated
 */
static void parser_characters(GMarkupParseContext *context, const char *text, gsize text_len, gpointer user_data, GError **error)
{
	(void)context;
	(void)user_data;
	(void)error;
	(void)text;
	(void)text_len;
}


/*
 * Called for strings that should be re-saved verbatim in this same
 * position, but are not otherwise interpretable. At the moment this
 * includes comments and processing instructions. Text is
 * nul-terminated.
 */
static void parser_passthrough(GMarkupParseContext *context, const char *passthrough_text, gsize text_len, gpointer user_data, GError **error)
{
	(void)context;
	(void)user_data;
	(void)error;
	(void)passthrough_text;
	(void)text_len;
}


/*
 * Called when any parsing method encounters an error. The GError should not be
 * freed.
 */
static void parser_error(GMarkupParseContext *context, GError *error, gpointer user_data)
{
	(void)context;
	(void)user_data;
	g_printerr("ERROR: %s\n", error->message);
}


/*
 * Parser
 */
static const GMarkupParser xml_parser = {
	parser_start_element,
	parser_end_element,
	parser_characters,
	parser_passthrough,
	parser_error
};

/*
 * XML for the parser.
 */
static char const preferences[] =
#include "preferences.xml.h"
;


static void cb_window_destroy(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	(void)widget;
	(void)event;
	(void)data;
	gtk_main_quit();
}


static gboolean cb_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	GuiWindow *gui = (GuiWindow *)data;

	(void)widget;
	if (event->keyval == GDK_Escape)
	{
		gui->exit_code = EXIT_WINDOW_CLOSED;
		gtk_main_quit();
	}
	return FALSE;
}


static void cb_ok(GtkWidget *widget, gpointer data)
{
	GuiWindow *gui = (GuiWindow *)data;

	(void)widget;
	gui->exit_code = EXIT_SUCCESS;
	gtk_main_quit();
}


static void cb_cancel(GtkWidget *widget, gpointer data)
{
	GuiWindow *gui = (GuiWindow *)data;

	(void)widget;
	gui->exit_code = EXIT_FAILURE;
	gtk_main_quit();
}


static void window_create(GuiWindow *gui)
{
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GtkWidget *hbox;
	GtkWidget *btn_box;
	GtkWidget *btn;

	gtk_window_set_default_icon_name(program_name);

	gui->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(gui->window), "destroy", G_CALLBACK(cb_window_destroy), gui);
	g_signal_connect(G_OBJECT(gui->window), "key_press_event", G_CALLBACK(cb_key_press), gui);
	gtk_window_set_title(GTK_WINDOW(gui->window), _("MagicOnLinux Settings"));
	gtk_window_set_accept_focus(GTK_WINDOW(gui->window), TRUE);
	gtk_container_set_border_width(GTK_CONTAINER(gui->window), 12);
	vbox = gtk_vbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(gui->window), vbox);

	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), 0);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, TRUE, TRUE, 0);
	gui->notebook = gtk_notebook_new();
	gtk_container_set_border_width(GTK_CONTAINER(gui->notebook), 6);
	gtk_box_pack_end(GTK_BOX(hbox), gui->notebook, FALSE, FALSE, 0);
	
	btn_box = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(btn_box), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(btn_box), 6);
	gtk_box_pack_end(GTK_BOX(vbox), btn_box, FALSE, FALSE, 0);
	btn = gtk_button_new_from_stock("gtk-ok");
	g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(cb_ok), gui);
	gtk_box_pack_start(GTK_BOX(btn_box), btn, FALSE, FALSE, 0);
	gtk_widget_grab_focus(btn);

	btn = gtk_button_new_from_stock("gtk-cancel");
	g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(cb_cancel), gui);
	gtk_box_pack_start(GTK_BOX(btn_box), btn, FALSE, FALSE, 0);
}


int main(int argc, char **argv)
{
	gboolean ok;
	GMarkupParseContext *context;
	GuiWindow gui;
	
	ok = gtk_init_check(&argc, &argv);

	if (!ok)
	{
		g_printerr("%s: unable to initialize GTK\n", program_name);
		return EXIT_FAILURE;
	}

	/* workaround for org.gtk.vfs.GoaVolumeMonitor sometimes hanging */
	unsetenv("DBUS_SESSION_BUS_ADDRESS");

	memset(&gui, 0, sizeof(gui));
	gui.exit_code = EXIT_WINDOW_CLOSED;
	window_create(&gui);

	context = g_markup_parse_context_new(&xml_parser, G_MARKUP_DEFAULT_FLAGS, &gui, NULL);

	ok = g_markup_parse_context_parse(context, preferences, sizeof(preferences) - 1, NULL);

	g_markup_parse_context_free(context);

	if (!ok)
	{
		g_printerr("Parsing ERROR\n");
		return EXIT_FAILURE;
	}

	/* open the window */
	gtk_widget_show_all(gui.window);
	gtk_main();

	return gui.exit_code;
}
