#define GDK_DISABLE_DEPRECATION_WARNINGS
#define GTK_DISABLE_DEPRECATION_WARNINGS
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#define EXIT_WINDOW_CLOSED (EXIT_FAILURE + EXIT_SUCCESS + 1)

static char const program_name[] = "mxgtk-settings";

typedef struct {
	GtkWidget *window;
	GtkWidget *notebook;
	
	char *section_name;
	GtkWidget *section_table;
	int section_row;
	GtkWidget *combo_box;

	GSList *widget_list;

	char *config_file;	

	int exit_code;
} GuiWindow;

#include "preferences.c"

/******************************************************************************/
/*** ---------------------------------------------------------------------- ***/
/******************************************************************************/

#if GTK_CHECK_VERSION(4, 0, 0)
static GtkWidget *gtk_vbox_new(gboolean homogeneous, int spacing)
{
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, spacing);
	gtk_box_set_homogeneous(GTK_BOX(box), homogeneous);
	return box;
}

/*** ---------------------------------------------------------------------- ***/

static GtkWidget *gtk_hbox_new(gboolean homogeneous, int spacing)
{
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, spacing);
	gtk_box_set_homogeneous(GTK_BOX(box), homogeneous);
	return box;
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

/*** ---------------------------------------------------------------------- ***/

static const char *gtk_entry_get_text(GtkEntry *entry)
{
	GtkEntryBuffer *buffer = gtk_entry_get_buffer(entry);
	return gtk_entry_buffer_get_text(buffer);
}

/*** ---------------------------------------------------------------------- ***/

static void gtk_entry_set_text(GtkEntry *entry, const char *text)
{
	GtkEntryBuffer *buffer = gtk_entry_get_buffer(entry);
	gtk_entry_buffer_set_text(buffer, text, strlen(text));
}


#define gtk_table_attach_defaults(table, w, left, right, top, bottom) gtk_table_attach(table, w, left, right, top, bottom, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0)
#define gtk_check_button_get_active(w) gtk_check_button_get_active(GTK_CHECK_BUTTON(w))
#define gtk_check_button_set_active(w, b) gtk_check_button_set_active(GTK_CHECK_BUTTON(w), b)

#else

#define gtk_check_button_get_active(w) gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))
#define gtk_check_button_set_active(w, b) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), b)

#endif

/******************************************************************************/
/*** ---------------------------------------------------------------------- ***/
/******************************************************************************/

/*
 * Utility functions
 */

static void widget_set_pref_value(GtkWidget *widget, const struct pref_val *val)
{
	g_object_set_data(G_OBJECT(widget), "pref-val", g_memdup(val, sizeof(*val)));
}

/*** ---------------------------------------------------------------------- ***/

static struct pref_val *widget_get_pref_value(GtkWidget *widget)
{
	return (struct pref_val *)g_object_get_data(G_OBJECT(widget), "pref-val");
}

/*** ---------------------------------------------------------------------- ***/

static void widget_set_section(GtkWidget *widget, const char *section)
{
	g_object_set_data(G_OBJECT(widget), "pref-section", g_strdup(section));
}

/*** ---------------------------------------------------------------------- ***/

static const char *widget_get_section(GtkWidget *widget)
{
	return (char *)g_object_get_data(G_OBJECT(widget), "pref-section");
}

/*** ---------------------------------------------------------------------- ***/

static char *path_expand(const char *value)
{
	if (value != NULL && value[0] == '~')
	{
		return g_strconcat(g_get_home_dir(), value + 1, NULL);
	} else
	{
		return g_strdup(value);
	}
}

/*** ---------------------------------------------------------------------- ***/

static char *path_shrink(const char *value)
{
	const char *home;
	size_t len;
	
	if (value == NULL)
		return g_strdup("");
	home = g_get_home_dir();
	len = strlen(home);
	if (strncmp(value, home, len) == 0)
		return g_strconcat("~", value + len, NULL);
	else
		return g_strdup(value);
}

/*** ---------------------------------------------------------------------- ***/

static void set_value(GtkWidget *w, int type, const char *value, int flags)
{
	struct pref_val val;
	
	memset(&val, 0, sizeof(val));
	val.type = type;
	switch (type)
	{
	case TYPE_NONE:
		return;
	case TYPE_STRING:
		if (value)
		{
			val.u.s = g_strdup(value);
		}
		break;
	case TYPE_BOOL:
		val.u.b = bool_from_string(value);
		break;
	case TYPE_PATH:
		if (value)
		{
			/* FIXME: filechooser does not select anything if file does not exist */
			val.u.p.p = path_expand(value);
		}
		val.u.p.flags = flags;
		break;
	case TYPE_FOLDER:
		if (value)
		{
			val.u.p.p = path_expand(value);
		}
		val.u.p.flags = flags;
		break;
	case TYPE_INT:
	case TYPE_UINT:
		if (value)
		{
			val.u.i.v = strtol(value, NULL, 0);
		}
		val.u.i.minval = gtk_adjustment_get_lower(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(w)));
		val.u.i.maxval = gtk_adjustment_get_upper(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(w)));
		break;
	case TYPE_CHOICE:
		if (value)
			val.u.c.c = strtol(value, NULL, 0);
		break;
	default:
		assert(0);
	}
	widget_set_pref_value(w, &val);
}

/******************************************************************************/
/*** ---------------------------------------------------------------------- ***/
/******************************************************************************/

/*
 * XML Parser
 */

static void cb_file_changed(GtkWidget *widget, gpointer data)
{
	struct pref_val *val;
	char *path;
	
	(void)data;
	val = widget_get_pref_value(widget);
#if GTK_CHECK_VERSION(4, 0, 0)
	{
		GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(widget));
		path = g_file_get_path(file);
		g_object_unref(G_OBJECT(file));
	}
#else
	path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));
#endif
	g_free(val->u.s);
	val->u.s = path;
}

/*** ---------------------------------------------------------------------- ***/

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
	int flags = NO_FLAGS;

	(void)context;
	(void)error;

	for (i = 0; attribute_names[i]; i++)
	{
		if (strcmp(attribute_names[i], "name") == 0)
			name = attribute_values[i];
		if (strcmp(attribute_names[i], "_label") == 0)
			display = attribute_values[i];
		if (strcmp(attribute_names[i], "default") == 0)
			default_value = attribute_values[i];
	}
	
	if (name == NULL)
	{
		g_printerr("%s missing name\n", element_name);
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
		gtk_widget_show(label);
		g_free(gui->section_name);
		gui->section_name = g_strdup(name);
#if GTK_CHECK_VERSION(4, 0, 0)
		gui->section_table = gtk_grid_new();
#else
		gui->section_table = gtk_table_new(2, 2, FALSE);
		gtk_table_set_row_spacings(GTK_TABLE(gui->section_table), 4);
		gtk_table_set_col_spacings(GTK_TABLE(gui->section_table), 4);
#endif
		gtk_widget_show(gui->section_table);
		gui->section_row = 0;
		if (strcmp(gui->section_name, "ADDITIONAL ATARI DRIVES") == 0)
		{
			/*
			 * put the path list in a scrolled window, it gets too large
			 */
#if GTK_CHECK_VERSION(4, 0, 0)
			GtkWidget *scroller = scroller = gtk_scrolled_window_new();

			gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), gui->section_table);
#else
			GtkWidget *scroller = scroller = gtk_scrolled_window_new(NULL, NULL);

			gtk_container_set_border_width(GTK_CONTAINER(scroller), 0);
			gtk_container_set_border_width(GTK_CONTAINER(scroller), 0);
			gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
			gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroller), GTK_SHADOW_ETCHED_IN);
			gtk_scrolled_window_set_placement(GTK_SCROLLED_WINDOW(scroller), GTK_CORNER_TOP_LEFT);
			gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroller), gui->section_table);
#endif
			gtk_notebook_append_page(GTK_NOTEBOOK(gui->notebook), scroller, label);
			gtk_widget_show(scroller);
		} else
		{
			vbox = gtk_vbox_new(FALSE, 0);
			gtk_widget_show(vbox);
			gtk_box_pack_start(GTK_BOX(vbox), gui->section_table, FALSE, FALSE, 0);
			gtk_notebook_append_page(GTK_NOTEBOOK(gui->notebook), vbox, label);
		}
	} else if (strcmp(element_name, "folder") == 0 ||
		strcmp(element_name, "path") == 0)
	{
		assert(gui->section_table != NULL);
		label = gtk_label_new(N_(display));
		gtk_widget_show(label);
		type = strcmp(element_name, "folder") == 0 ? TYPE_FOLDER : TYPE_PATH;
		flags = NO_FLAGS;
		for (i = 0; attribute_names[i]; i++)
		{
			if (strcmp(attribute_names[i], "flags") == 0)
				flags = strtol(attribute_values[i], NULL, 0);
		}
#if GTK_CHECK_VERSION(4, 0, 0)
		gtk_grid_attach(GTK_GRID(gui->section_table), label, 0, 1, gui->section_row, gui->section_row + 1);
		if (type == TYPE_FOLDER)
			button = gtk_button_new_with_label(_("Select a Folder"));
		else
			button = gtk_button_new_with_label(_("Select a File"));
		gtk_grid_attach(GTK_GRID(gui->section_table), button, 1, 2, gui->section_row, gui->section_row + 1);
		(void)cb_file_changed;
#else
		gtk_table_attach_defaults(GTK_TABLE(gui->section_table), label, 0, 1, gui->section_row, gui->section_row + 1);
		if (type == TYPE_FOLDER)
			button = gtk_file_chooser_button_new(_("Select a Folder"), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
		else
			button = gtk_file_chooser_button_new(_("Select a File"), GTK_FILE_CHOOSER_ACTION_OPEN);
		gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(button), TRUE);
		g_signal_connect(G_OBJECT(button), "file-set", G_CALLBACK(cb_file_changed), gui);
		gtk_table_attach_defaults(GTK_TABLE(gui->section_table), button, 1, 2, gui->section_row, gui->section_row + 1);
#endif
		gtk_widget_show(button);
		settings_widget = button;
		if (strcmp(name, "atari_drv_c") == 0 ||
			strcmp(name, "atari_drv_h") == 0 ||
			strcmp(name, "atari_drv_m") == 0)
			gtk_widget_set_sensitive(button, FALSE);
		if (flags != NO_FLAGS)
		{
			char *button_name;
			
			gui->section_row++;
			vbox = gtk_hbox_new(FALSE, 0);
			gtk_widget_show(vbox);
#if GTK_CHECK_VERSION(4, 0, 0)
			gtk_grid_attach(GTK_GRID(gui->section_table), vbox, 1, 3, gui->section_row, gui->section_row + 1);
#else
			gtk_table_attach_defaults(GTK_TABLE(gui->section_table), vbox, 1, 3, gui->section_row, gui->section_row + 1);
#endif
			
			button_name = g_strconcat(name, "_rdonly", NULL);
			button = gtk_check_button_new_with_label(_("read-only"));
			gtk_widget_show(button);
			gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
			gtk_widget_set_name(button, button_name);
			g_object_set_data(G_OBJECT(settings_widget), button_name, button);
			
			button_name = g_strconcat(name, "_dosnames", NULL);
			button = gtk_check_button_new_with_label(_("DOS 8+3 format"));
			gtk_widget_show(button);
			gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
			gtk_widget_set_name(button, button_name);
			g_object_set_data(G_OBJECT(settings_widget), button_name, button);
			
			button_name = g_strconcat(name, "_insensitive", NULL);
			button = gtk_check_button_new_with_label(_("case insensitive"));
			gtk_widget_show(button);
			gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
			gtk_widget_set_name(button, button_name);
			g_object_set_data(G_OBJECT(settings_widget), button_name, button);
		}
		gui->section_row++;
	} else if (strcmp(element_name, "string") == 0)
	{
		assert(gui->section_table != NULL);
		label = gtk_label_new(N_(display));
		gtk_widget_show(label);
#if GTK_CHECK_VERSION(4, 0, 0)
		gtk_grid_attach(GTK_GRID(gui->section_table), label, 0, 1, gui->section_row, gui->section_row + 1);
#else
		gtk_table_attach_defaults(GTK_TABLE(gui->section_table), label, 0, 1, gui->section_row, gui->section_row + 1);
#endif
		entry = gtk_entry_new();
		gtk_widget_show(entry);
#if GTK_CHECK_VERSION(4, 0, 0)
		gtk_grid_attach(GTK_GRID(gui->section_table), entry, 1, 2, gui->section_row, gui->section_row + 1);
#else
		gtk_table_attach_defaults(GTK_TABLE(gui->section_table), entry, 1, 2, gui->section_row, gui->section_row + 1);
#endif
		gui->section_row++;
		settings_widget = entry;
		type = TYPE_STRING;
	} else if (strcmp(element_name, "int") == 0)
	{
		long minval = 0;
		long maxval = LONG_MAX;
		long step = 1;
		GtkAdjustment *adjustment;
		
		assert(gui->section_table != NULL);
		label = gtk_label_new(N_(display));
		gtk_widget_show(label);
		for (i = 0; attribute_names[i]; i++)
		{
			if (strcmp(attribute_names[i], "minval") == 0)
				minval = strtol(attribute_values[i], NULL, 0);
			if (strcmp(attribute_names[i], "maxval") == 0)
				maxval = strtol(attribute_values[i], NULL, 0);
			if (strcmp(attribute_names[i], "step") == 0)
				step = strtol(attribute_values[i], NULL, 0);
		}
		adjustment = (GtkAdjustment *)gtk_adjustment_new(minval, minval, maxval, step, step * 16, 0);
#if GTK_CHECK_VERSION(4, 0, 0)
		gtk_grid_attach(GTK_GRID(gui->section_table), label, 0, 1, gui->section_row, gui->section_row + 1);
#else
		gtk_table_attach_defaults(GTK_TABLE(gui->section_table), label, 0, 1, gui->section_row, gui->section_row + 1);
#endif
		button = gtk_spin_button_new(GTK_ADJUSTMENT(adjustment), 0, 0);
		gtk_widget_show(button);
		gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(button), TRUE);
#if GTK_CHECK_VERSION(4, 0, 0)
		gtk_grid_attach(GTK_GRID(gui->section_table), button, 1, 2, gui->section_row, gui->section_row + 1);
#else
		gtk_table_attach_defaults(GTK_TABLE(gui->section_table), button, 1, 2, gui->section_row, gui->section_row + 1);
#endif
		gui->section_row++;
		settings_widget = button;
		type = TYPE_INT;
		/*
		 * some values must be written as unsigned, or the application fails to parse them
		 */
		if (strncmp(name, "app_window_", 11) == 0)
			type = TYPE_UINT;
	} else if (strcmp(element_name, "bool") == 0)
	{
		assert(gui->section_table != NULL);
		button = gtk_check_button_new_with_label(N_(display));
		gtk_widget_show(button);
#if GTK_CHECK_VERSION(4, 0, 0)
		gtk_grid_attach(GTK_GRID(gui->section_table), button, 1, 2, gui->section_row, gui->section_row + 1);
#else
		gtk_table_attach_defaults(GTK_TABLE(gui->section_table), button, 1, 2, gui->section_row, gui->section_row + 1);
#endif
		gui->section_row++;
		settings_widget = button;
		type = TYPE_BOOL;
	} else if (strcmp(element_name, "choice") == 0)
	{
		assert(gui->section_table != NULL);
		assert(gui->combo_box == NULL);
		label = gtk_label_new(N_(display));
		gtk_widget_show(label);
#if GTK_CHECK_VERSION(4, 0, 0)
		gtk_grid_attach(GTK_GRID(gui->section_table), label, 0, 1, gui->section_row, gui->section_row + 1);
#else
		gtk_table_attach_defaults(GTK_TABLE(gui->section_table), label, 0, 1, gui->section_row, gui->section_row + 1);
#endif
#if GTK_CHECK_VERSION(3, 0, 0)
		gui->combo_box = gtk_combo_box_text_new();
#else
		gui->combo_box = gtk_combo_box_new_text();
#endif
		gtk_widget_show(gui->combo_box);
#if GTK_CHECK_VERSION(4, 0, 0)
		gtk_grid_attach(GTK_GRID(gui->section_table), gui->combo_box, 1, 2, gui->section_row, gui->section_row + 1);
#else
		gtk_table_attach_defaults(GTK_TABLE(gui->section_table), gui->combo_box, 1, 2, gui->section_row, gui->section_row + 1);
#endif
		gui->section_row++;
		settings_widget = gui->combo_box;
		type = TYPE_CHOICE;
	} else if (strcmp(element_name, "select") == 0)
	{
		assert(gui->section_table != NULL);
		assert(gui->combo_box != NULL);
#if GTK_CHECK_VERSION(3, 0, 0)
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gui->combo_box), N_(display));
#else
		gtk_combo_box_append_text(GTK_COMBO_BOX(gui->combo_box), N_(display));
#endif
	} else
	{
		g_printerr("unsupported element %s\n", element_name);
	}
	
	if (settings_widget)
	{
		widget_set_section(settings_widget, gui->section_name);
		gtk_widget_set_name(settings_widget, g_strdup(name));
		set_value(settings_widget, type, default_value, flags);
		gui->widget_list = g_slist_append(gui->widget_list, settings_widget);
	}
}

/*** ---------------------------------------------------------------------- ***/

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
		g_free(gui->section_name);
		gui->section_name = NULL;
	} else if (strcmp(element_name, "choice") == 0)
	{
		struct pref_val *val;

		assert(gui->combo_box != NULL);
		val = widget_get_pref_value(gui->combo_box);
		val->u.c.minval = 0;
		val->u.c.maxval = gtk_tree_model_iter_n_children(gtk_combo_box_get_model(GTK_COMBO_BOX(gui->combo_box)), NULL) - 1;
		gui->combo_box = NULL;
	}
}

/*** ---------------------------------------------------------------------- ***/

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

/*** ---------------------------------------------------------------------- ***/

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

/*** ---------------------------------------------------------------------- ***/

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

/*** ---------------------------------------------------------------------- ***/

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

/******************************************************************************/
/*** ---------------------------------------------------------------------- ***/
/******************************************************************************/

/*
 * Read/write preferences
 */

/** **********************************************************************************************
 *
 * @brief Write configuration file
 *
 * @param[in] cfgfile path
 *
 * @return TRUE/FALSE
 *
 ************************************************************************************************/
static gboolean writePreferences(GuiWindow *gui)
{
	FILE *f;
	GtkWidget *w;
	GSList *l;
	const char *last_section = 0;
	const char *section;
	const char *name;
	struct pref_val *val;
	char *str;

	f = fopen(gui->config_file, "w");
	if (f == NULL)
	{
		/* FIXME: use dialog */
		g_printerr("can't create %s\n", gui->config_file);
		return FALSE;
	}
	/* g_slist_foreach(gui->widget_list) */
	for (l = gui->widget_list; l; l = l->next)
	{
		w = l->data;
		val = widget_get_pref_value(w);
		section = widget_get_section(w);
		name = gtk_widget_get_name(w);
		if (last_section == NULL || strcmp(last_section, section) != 0)
		{
			fprintf(f, "[%s]\n", section);
			last_section = section;
			if (strcmp(section, "ADDITIONAL ATARI DRIVES") == 0)
			{
				fputs("# atari_drv_<A..T,V..Z> = flags [1:read-only, 2:8+3, 4:case-insensitive] path or image\n", f);
			}
		}
		switch (val->type)
		{
		case TYPE_NONE:
			break;
		case TYPE_PATH:
		case TYPE_FOLDER:
			if (val->u.p.p != NULL && *val->u.p.p != '\0')
			{
				str = path_shrink(val->u.p.p);
				if (val->u.p.flags == NO_FLAGS)
					fprintf(f, "%s = \"%s\"\n", name, str);
				else
					fprintf(f, "%s = %d \"%s\"\n", name, val->u.p.flags, str);
				g_free(str);
			}
			break;
		case TYPE_STRING:
			if (val->u.s != NULL && *val->u.s != '\0')
				fprintf(f, "%s = \"%s\"\n", name, val->u.s ? val->u.s : "");
			break;
		case TYPE_INT:
			fprintf(f, "%s = %ld\n", name, val->u.i.v);
			break;
		case TYPE_UINT:
			fprintf(f, "%s = %u\n", name, (unsigned int)val->u.i.v);
			break;
		case TYPE_BOOL:
			fprintf(f, "%s = %s\n", name, bool_to_string(val->u.b));
			break;
		case TYPE_CHOICE:
			fprintf(f, "%s = %d\n", name, val->u.c.c);
			if (strcmp(name, "atari_screen_colour_mode") == 0)
				fputs("# 0:24b 1:16b 2:256 3:16 4:16ip 5:4ip 6:mono\n", f);
			break;
		default:
			assert(0);
		}
	}
	return TRUE;
}

/** **********************************************************************************************
 *
 * @brief Update current preferences from GUI
 *
 * @param[in] cfgfile  path
 *
 * @return TRUE/FALSE
 *
 ************************************************************************************************/
static gboolean updatePreferences(GuiWindow *gui)
{
	GtkWidget *w;
	GSList *l;
	struct pref_val *val;

	/* g_slist_foreach(gui->widget_list) */
	for (l = gui->widget_list; l; l = l->next)
	{
		w = l->data;
		val = widget_get_pref_value(w);
		switch (val->type)
		{
		case TYPE_NONE:
			break;
		case TYPE_PATH:
		case TYPE_FOLDER:
			/* pathname already done in cb_file_changed */
			if (val->u.p.flags != NO_FLAGS)
			{
				char *button_name;
				GtkWidget *button;
				const char *name = gtk_widget_get_name(w);
				
				val->u.p.flags = 0;
				button_name = g_strconcat(name, "_rdonly", NULL);
				button = g_object_get_data(G_OBJECT(w), button_name);
				if (gtk_check_button_get_active(button))
					val->u.p.flags |= DRV_FLAG_RDONLY;
				g_free(button_name);

				button_name = g_strconcat(name, "_dosnames", NULL);
				button = g_object_get_data(G_OBJECT(w), button_name);
				if (gtk_check_button_get_active(button))
					val->u.p.flags |= DRV_FLAG_8p3;
				g_free(button_name);

				button_name = g_strconcat(name, "_insensitive", NULL);
				button = g_object_get_data(G_OBJECT(w), button_name);
				if (gtk_check_button_get_active(button))
					val->u.p.flags |= DRV_FLAG_CASE_INSENS;
				g_free(button_name);
			}
			break;
		case TYPE_STRING:
			g_free(val->u.s);
			val->u.s = g_strdup(gtk_entry_get_text(GTK_ENTRY(w)));
			break;
		case TYPE_INT:
		case TYPE_UINT:
			val->u.i.v = gtk_spin_button_get_value(GTK_SPIN_BUTTON(w));
			break;
		case TYPE_BOOL:
			val->u.b = gtk_check_button_get_active(w);
			break;
		case TYPE_CHOICE:
			val->u.c.c = gtk_combo_box_get_active(GTK_COMBO_BOX(w));
			break;
		default:
			assert(0);
		}
	}
	return TRUE;
}

/** **********************************************************************************************
 *
 * @brief Populate GUI from current preferences
 *
 * @param[in] cfgfile  path
 *
 * @return TRUE/FALSE
 *
 ************************************************************************************************/
static gboolean populatePreferences(GuiWindow *gui)
{
	GtkWidget *w;
	GSList *l;
	struct pref_val *val;

	/* g_slist_foreach(gui->widget_list) */
	for (l = gui->widget_list; l; l = l->next)
	{
		w = l->data;
		val = widget_get_pref_value(w);
		switch (val->type)
		{
		case TYPE_NONE:
			break;
		case TYPE_PATH:
		case TYPE_FOLDER:
			if (val->u.p.p)
			{
#if GTK_CHECK_VERSION(4, 0, 0)
				{
					GFile *file = g_file_new_for_path(val->u.p.p);
					if (val->type == TYPE_PATH)
						gtk_file_chooser_set_file(GTK_FILE_CHOOSER(w), file, NULL);
					else
						gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(w), file, NULL);
				}
#else
				if (val->type == TYPE_PATH)
					gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(w), val->u.p.p);
				else
					gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(w), val->u.p.p);
#endif
			}
			if (val->u.p.flags != NO_FLAGS)
			{
				char *button_name;
				GtkWidget *button;
				const char *name = gtk_widget_get_name(w);
				
				button_name = g_strconcat(name, "_rdonly", NULL);
				button = g_object_get_data(G_OBJECT(w), button_name);
				gtk_check_button_set_active(button, (val->u.p.flags & DRV_FLAG_RDONLY) != 0);
				g_free(button_name);

				button_name = g_strconcat(name, "_dosnames", NULL);
				button = g_object_get_data(G_OBJECT(w), button_name);
				gtk_check_button_set_active(button, (val->u.p.flags & DRV_FLAG_8p3) != 0);
				g_free(button_name);

				button_name = g_strconcat(name, "_insensitive", NULL);
				button = g_object_get_data(G_OBJECT(w), button_name);
				gtk_check_button_set_active(button, (val->u.p.flags & DRV_FLAG_CASE_INSENS) != 0);
				g_free(button_name);
			}
			break;
		case TYPE_STRING:
			if (val->u.s)
			{
				gtk_entry_set_text(GTK_ENTRY(w), val->u.s);
			}
			break;
		case TYPE_INT:
		case TYPE_UINT:
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), val->u.i.v);
			break;
		case TYPE_BOOL:
			gtk_check_button_set_active(w, val->u.b);
			break;
		case TYPE_CHOICE:
			gtk_combo_box_set_active(GTK_COMBO_BOX(w), val->u.c.c);
			break;
		default:
			assert(0);
		}
	}
	return TRUE;
}

/** **********************************************************************************************
 *
 * @brief Read string, optionally enclosed in "" or ''
 *
 * @param[in_out] in input line pointer, will be advanced accordingly
 *
 * @return str or NULL for error
 *
 ************************************************************************************************/

static char *eval_quotated_str(const char **in)
{
	GString *text;
	const char *in_start = *in;
	char delimiter = **in;
	char *str;

	text = g_string_new("");

	if (delimiter == '\"' || delimiter == '\'')
	{
		in_start++;
	} else
	{
		delimiter = 0;
	}
	while (*in_start != '\0' && *in_start != '\n' && *in_start != delimiter)
	{
		g_string_append_c(text, *in_start);
		in_start++;
	}

	str = g_string_free(text, FALSE);
	if (delimiter != 0)
	{
		if (*in_start == delimiter)
		{
			in_start++;
		} else
		{
			g_free(str);
			str = NULL;	/* missing delimiter */
		}
	}
	*in = in_start;

	return str;
}

/** **********************************************************************************************
 *
 * @brief Read string, optionally enclosed in "" or '', and evaluates leading '~' for user home directory
 *
 * @param[out]    outbuf   output buffer, holds raw string
 * @param[in]     bufsiz   size of output buffer including end-of-string
 * @param[in_out] in       input line pointer, will be advanced accordingly
 *
 * @return str or NULL for error
 *
 ************************************************************************************************/
static char *eval_quotated_str_path(const char **in)
{
	char *str = eval_quotated_str(in);
	if (str != NULL)
	{
		char *value = path_expand(str);
		g_free(str);
		str = value;
	}
	return str;
}


/** **********************************************************************************************
 *
 * @brief Read numerical value, decimal, sedecimal or octal
 *
 * @param[out] outval   number
 * @param[in]  minval   minimum valid number
 * @param[in]  maxval   maximum valid number
 * @param[in_out] in    input line pointer, will be advanced accordingly
 *
 * @return TRUE for OK or FALSE for error
 *
 ************************************************************************************************/

static gboolean eval_int(long *outval, long minval, long maxval, const char **in)
{
	char *endptr;
	long value;
	
	if (**in == '-')
	{
		value = strtol(*in, &endptr, 0 /*auto base*/);
	} else
	{
		value = strtoul(*in, &endptr, 0 /*auto base*/);
		if ((unsigned long)value > INT32_MAX)
		{
			value = value - UINT32_MAX - 1;
		}
	}
	if (endptr > *in)
	{
		if (maxval > minval && (value < minval || value > maxval))
		{
			g_printerr("value %ld out of range %ld..%ld\n", value, minval, maxval);
			return FALSE;
		}
		*outval = value;
		*in = endptr;
		return TRUE;
	}
	return FALSE;
}

/** **********************************************************************************************
 *
 * @brief Evaluate boolean string "yes" or "no", optionally enclosed in "" or ''
 *
 * @param[out]    outval   boolean value
 * @param[in_out] in       input line pointer, will be advanced accordingly
 *
 * @return 0 for OK or 1 for error
 *
 ************************************************************************************************/
static gboolean eval_bool(gboolean *outval, const char **line)
{
	char *YesOrNo;
	gboolean ok;

	YesOrNo = eval_quotated_str(line);
	ok = YesOrNo != NULL;
	if (ok)
	{
		*outval = bool_from_string(YesOrNo);
	}
	g_free(YesOrNo);

	return ok;
}

/** **********************************************************************************************
 *
 * @brief Evaluate a single preferences line
 *
 * @param[in]  line   input line, with trailing \n and zero byte
 *
 * @return TRUE for OK, FALSE for error
 *
 * @note empty lines, sections and comments have already been processed
 *
 ************************************************************************************************/

static gboolean evaluatePreferencesLine(GuiWindow *gui, const char *line)
{
	GtkWidget *w;
	GSList *l;
	const char *key;
	size_t keylen;
	struct pref_val *val;
	gboolean ok = TRUE;
	char *str;
	long lv;

	/* g_slist_foreach(gui->widget_list) */
	for (l = gui->widget_list; l; l = l->next)
	{
		w = l->data;
		val = widget_get_pref_value(w);
		key = gtk_widget_get_name(w);
		keylen = strlen(key);
		if (g_ascii_strncasecmp(line, key, keylen) == 0 && (g_ascii_isspace(line[keylen]) || line[keylen] == '='))
		{
			line += keylen;
			break;
		}
	}
	if (l == NULL)
	{
		g_printerr("unknown key\n");
		return FALSE;
	}
	
	/* skip spaces */
	while (g_ascii_isspace(*line))
	{
		line++;
	}

	if (*line != '=')
	{
		return FALSE;
	}
	line++;

	/* skip spaces */
	while (g_ascii_isspace(*line))
	{
		line++;
	}

	switch (val->type)
	{
	case TYPE_PATH:
	case TYPE_FOLDER:
		if (val->u.p.flags != NO_FLAGS)
		{
			lv = 0;
			ok &= eval_int(&lv, 0, 0, &line);
			if (!ok)
				break;
			val->u.p.flags = lv;
			while (g_ascii_isspace(*line))
				line++;
		}
		str = eval_quotated_str_path(&line);
		if (str != NULL)
		{
			g_free(val->u.p.p);
			val->u.p.p = str;
		} else
		{
			ok = FALSE;
		}
		break;

	case TYPE_STRING:
		str = eval_quotated_str(&line);
		if (str != NULL)
		{
			g_free(val->u.s);
			val->u.s = str;
			gtk_entry_set_text(GTK_ENTRY(w), str);
		} else
		{
			ok = FALSE;
		}
		break;

	case TYPE_BOOL:
		ok &= eval_bool(&val->u.b, &line);
		break;

	case TYPE_INT:
	case TYPE_UINT:
		ok &= eval_int(&val->u.i.v, val->u.i.minval, val->u.i.maxval, &line);
		break;

	case TYPE_CHOICE:
		ok &= eval_int(&lv, val->u.c.minval, val->u.c.maxval, &line);
		if (ok)
			val->u.c.c = lv;
		break;
	default:
		assert(0);
	}

	if (ok)
	{
		/* skip trailing blanks */
		while (g_ascii_isspace(*line))
		{
			line++;
		}
		if (*line != '\0' && *line != '\n')
		{
			ok = FALSE;   /* rubbish at end of line */
		}
	}

	return ok;
}

/** **********************************************************************************************
 *
 * @brief Read all preferences from a configuration file or write default file, if requested
 *
 * @param[in]  gui    GUI parameters
 *
 * @return number of errors
 *
 ************************************************************************************************/
static gboolean getPreferences(GuiWindow *gui)
{
	struct stat statbuf;
	FILE *f;
	char line[2048];
	gboolean ok = TRUE;
	gboolean nline_ok;

	if (stat(gui->config_file, &statbuf) != 0)
	{
		/* Configuration file does not exist. Use defaults. */
		return TRUE;
	}

	f = fopen(gui->config_file, "r");
	if (f == NULL)
	{
		return FALSE;
	}

	while (fgets(line, sizeof(line) - 1, f))
	{
		const char *c = line;

		/* skip spaces */
		while (g_ascii_isspace(*c))
		{
			c++;
		}

		/* skip comments */
		if (*c == '#')
		{
			continue;
		}

		/* skip section names */
		if (*c == '[')
		{
			continue;
		}

		/* skip empty lines */
		if (*c == '\0')
		{
			continue;
		}

		nline_ok = evaluatePreferencesLine(gui, c);
		if (!nline_ok)
		{
			/* FIXME: use dialog */
			g_printerr("Syntax error in configuration file: %s", line);
		}
		ok &= nline_ok;
	}
	fclose(f);
	return ok;
}

/******************************************************************************/
/*** ---------------------------------------------------------------------- ***/
/******************************************************************************/

/*
 * GUI
 */

#if !GTK_CHECK_VERSION(4, 0, 0)
static gboolean cb_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	GuiWindow *gui = (GuiWindow *)data;

	(void)widget;
	if (event->keyval == GDK_KEY_Escape)
	{
		gui->exit_code = EXIT_WINDOW_CLOSED;
		gtk_main_quit();
	}
	return FALSE;
}
#endif

/*** ---------------------------------------------------------------------- ***/

static void mxgtk_quit(GuiWindow *gui)
{
#if GTK_CHECK_VERSION(4, 0, 0)
	exit(gui->exit_code);
#else
	(void)gui;
	gtk_main_quit();
#endif
}

/*** ---------------------------------------------------------------------- ***/

static void cb_ok(GtkWidget *widget, gpointer data)
{
	GuiWindow *gui = (GuiWindow *)data;

	(void)widget;
	updatePreferences(gui);
	if (writePreferences(gui))
	{
		gui->exit_code = EXIT_SUCCESS;
		mxgtk_quit(gui);
	}
}

/*** ---------------------------------------------------------------------- ***/

static void cb_cancel(GtkWidget *widget, gpointer data)
{
	GuiWindow *gui = (GuiWindow *)data;

	(void)widget;
	gui->exit_code = EXIT_FAILURE;
	mxgtk_quit(gui);
}

/*** ---------------------------------------------------------------------- ***/

static void cb_window_destroy(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GuiWindow *gui = (GuiWindow *)data;
	(void)widget;
	(void)event;
	mxgtk_quit(gui);
}

/*** ---------------------------------------------------------------------- ***/

static void window_create(GuiWindow *gui)
{
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GtkWidget *hbox;
	GtkWidget *btn_box;
	GtkWidget *btn;

	gtk_window_set_default_icon_name(program_name);

#if GTK_CHECK_VERSION(4, 0, 0)
	gui->window = gtk_window_new();
#else
	gui->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_accept_focus(GTK_WINDOW(gui->window), TRUE);
	gtk_container_set_border_width(GTK_CONTAINER(gui->window), 12);
#endif
	g_signal_connect(G_OBJECT(gui->window), "destroy", G_CALLBACK(cb_window_destroy), gui);
#if !GTK_CHECK_VERSION(4, 0, 0)
	g_signal_connect(G_OBJECT(gui->window), "key_press_event", G_CALLBACK(cb_key_press), gui);
#endif
	gtk_window_set_title(GTK_WINDOW(gui->window), _("MagicOnLinux Settings"));
	vbox = gtk_vbox_new(FALSE, 12);
	gtk_widget_show(vbox);
#if GTK_CHECK_VERSION(4, 0, 0)
	gtk_window_set_child(GTK_WINDOW(gui->window), vbox);
#else
	gtk_container_add(GTK_CONTAINER(gui->window), vbox);
#endif

	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox2);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, TRUE, TRUE, 0);
#if !GTK_CHECK_VERSION(4, 0, 0)
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), 0);
#endif

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, TRUE, TRUE, 0);
	gui->notebook = gtk_notebook_new();
	gtk_widget_show(gui->notebook);
#if !GTK_CHECK_VERSION(4, 0, 0)
	gtk_container_set_border_width(GTK_CONTAINER(gui->notebook), 6);
#endif
	gtk_box_pack_end(GTK_BOX(hbox), gui->notebook, FALSE, FALSE, 0);
	
#if GTK_CHECK_VERSION(4, 0, 0)
	btn_box = gtk_hbox_new(FALSE, 0);
#else
	btn_box = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(btn_box), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(btn_box), 6);
#endif
	gtk_box_pack_end(GTK_BOX(vbox), btn_box, FALSE, FALSE, 0);
	gtk_widget_show(btn_box);

#if GTK_CHECK_VERSION(4, 0, 0)
	btn = gtk_button_new_from_icon_name("gtk-cancel");
	gtk_button_set_label(GTK_BUTTON(btn), _("Cancel"));
#else
	btn = gtk_button_new_from_stock("gtk-cancel");
#endif
	gtk_widget_show(btn);
	g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(cb_cancel), gui);
	gtk_box_pack_start(GTK_BOX(btn_box), btn, FALSE, FALSE, 0);

#if GTK_CHECK_VERSION(4, 0, 0)
	btn = gtk_button_new_from_icon_name("gtk-ok");
	gtk_button_set_label(GTK_BUTTON(btn), _("Ok"));
#else
	btn = gtk_button_new_from_stock("gtk-ok");
#endif
	gtk_widget_show(btn);
	g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(cb_ok), gui);
	gtk_box_pack_start(GTK_BOX(btn_box), btn, FALSE, FALSE, 0);
	gtk_widget_grab_focus(btn);

#if 0
	{
		gint win_w, win_h;
		win_w = gdk_screen_width() * 0.7;
		win_h = gdk_screen_height() * 0.7;
		gtk_window_set_default_size(GTK_WINDOW(gui->window), win_w, win_h);
	}
#endif
}

/*** ---------------------------------------------------------------------- ***/

int main(int argc, char **argv)
{
	gboolean ok;
	GMarkupParseContext *context;
	GuiWindow gui;
	
#if GTK_CHECK_VERSION(4, 0, 0)
	gtk_init();
	(void)argc;
	(void)argv;
#else
	ok = gtk_init_check(&argc, &argv);
	if (!ok)
	{
		g_printerr("%s: unable to initialize GTK\n", program_name);
		return EXIT_FAILURE;
	}
#endif

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

#if defined(__APPLE__)
	gui.config_file = path_expand("~/Library/Preferences/magiclinux.conf");
#else
	gui.config_file = path_expand("~/.config/magiclinux.conf");
#endif
	getPreferences(&gui);
	populatePreferences(&gui);

	/* open the window */
	gtk_widget_show(gui.window);
#if GTK_CHECK_VERSION(4, 0, 0)
	while (g_list_model_get_n_items(gtk_window_get_toplevels()) > 0)
		g_main_context_iteration(NULL, TRUE);
#else
	gtk_main();
#endif

	return gui.exit_code;
}
