#define GDK_DISABLE_DEPRECATION_WARNINGS
#define GTK_DISABLE_DEPRECATION_WARNINGS
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <sys/stat.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <gtk/gtk.h>
#pragma GCC diagnostic pop
#include <locale.h>
#include <gdk/gdkkeysyms.h>
#include "mxnls.h"
#include "country.c"

#define _STRINGIFY1(x) #x
#define _STRINGIFY(x) _STRINGIFY1(x)

#undef g_utf8_next_char
#define g_utf8_next_char(p) ((p) + g_utf8_skip[(unsigned char)(*p)])

#define UNUSED(x) (void)(x)

#define EXIT_WINDOW_CLOSED (EXIT_FAILURE + EXIT_SUCCESS + 1)

static char const program_name[] = "mxgtk-settings";
static char const program_version[] = "1.0";

enum {
	TYPE_NONE,
	TYPE_PATH,
	TYPE_FOLDER,
	TYPE_STRING,
	TYPE_INT,
	TYPE_UINT,
	TYPE_BOOL,
	TYPE_CHOICE
};

enum {
	CHOICE_COL_TEXT,
	CHOICE_COL_ORIG_TEXT,
	CHOICE_COL_VALUE,
	CHOICE_NUM_COLS
};

struct pref_val {
	int type;
	gboolean gui_only;
	union {
		struct {
			char *s;
			char *default_value;
			char *orig_value;
		} s;
		struct {
			char *p;
			int flags;
			char *default_value;
			int default_flags;
			char *orig_value;
			int orig_flags;
#define NO_FLAGS -1
#define DRV_FLAG_RDONLY         1   /* read-only */
#define DRV_FLAG_8p3            2   /* filenames in 8+3 format, uppercase */
#define DRV_FLAG_CASE_INSENS    4   /* case insensitive, e.g. (V)FAT or HFS(+) */
		} p;
		struct {
			long v;
			long minval;
			long maxval;
			long default_value;
			long orig_value;
		} i;
		double d;
		struct {
			gboolean b;
			gboolean default_value;
			gboolean orig_value;
		} b;
		struct {
			int c;
			int minval;
			int maxval;
			int orig_value;
			int default_value;
		} c;
	} u;
};

typedef struct {
	GtkWidget *window;
	GtkWidget *notebook;
	
	char *section_name;
	GtkWidget *section_table;
	int section_row;
	GtkWidget *combo_box;
	GtkWidget *pref_show_tooltips;
	GtkWidget *ok_button;
	GtkWidget *cancel_button;

	GSList *widget_list;
	GSList *translation_list;
	GSList *section_names;

	char *config_file;	

	int exit_code;
} GuiWindow;

/******************************************************************************/
/*** ---------------------------------------------------------------------- ***/
/******************************************************************************/

/*
 * Utility functions
 */
static gboolean bool_from_string(const char *str)
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

/*** ---------------------------------------------------------------------- ***/

static const char *bool_to_string(gboolean b)
{
	return b ? "YES" : "NO";
}

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
	UNUSED(padding);
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
	UNUSED(padding);
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

/*** ---------------------------------------------------------------------- ***/

static char *capitalize_string(const char *str)
{
	char first[8] = "";

	if (!str)
		return NULL;

	g_unichar_to_utf8(g_unichar_totitle(g_utf8_get_char(str)), first);
	str = g_utf8_next_char(str);
	return g_strconcat(first, str, NULL);
}

/*** ---------------------------------------------------------------------- ***/

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

static void widget_set_tooltip(GtkWidget *widget, const char *tooltip)
{
	if (tooltip)
	{
		g_object_set_data(G_OBJECT(widget), "pref-tooltip", g_strdup(tooltip));
		gtk_widget_set_tooltip_text(widget, _(tooltip));
	}
}

/*** ---------------------------------------------------------------------- ***/

static const char *widget_get_tooltip(GtkWidget *widget)
{
	return g_object_get_data(G_OBJECT(widget), "pref-tooltip");
}

/*** ---------------------------------------------------------------------- ***/

#ifdef ENABLE_NLS
static void widget_set_translation(GtkWidget *widget, const char *orig_text)
{
	if (orig_text)
	{
		g_object_set_data(G_OBJECT(widget), "pref-translation", g_strdup(orig_text));
	}
}

/*** ---------------------------------------------------------------------- ***/

static const char *widget_get_translation(GtkWidget *widget)
{
	return (char *)g_object_get_data(G_OBJECT(widget), "pref-translation");
}
#else
#define widget_set_translation(w, t)
#define widget_get_translation(w) NULL
#endif

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
	if (len > 0 && (home[len - 1] == '/' || home[len - 1] == '\\'))
		len--;
	if (len > 0 && strncmp(value, home, len) == 0)
		return g_strconcat("~", value + len, NULL);
	return g_strdup(value);
}

/*** ---------------------------------------------------------------------- ***/

static void set_value(GtkWidget *w, int type, const char *value, int flags, gboolean gui_only)
{
	struct pref_val val;
	
	memset(&val, 0, sizeof(val));
	val.type = type;
	val.gui_only = gui_only;
	switch (type)
	{
	case TYPE_NONE:
		return;
	case TYPE_STRING:
		if (value)
		{
			val.u.s.s = g_strdup(value);
			val.u.s.default_value = g_strdup(value);
			val.u.s.orig_value = g_strdup(value);
		}
		break;
	case TYPE_BOOL:
		val.u.b.b = bool_from_string(value);
		val.u.b.default_value = val.u.b.b;
		val.u.b.orig_value = val.u.b.b;
		break;
	case TYPE_PATH:
		if (value)
		{
			/* FIXME: filechooser does not select anything if file does not exist */
			val.u.p.p = path_expand(value);
		}
		val.u.p.flags = flags;
		val.u.p.default_value = g_strdup(val.u.p.p);
		val.u.p.default_flags = val.u.p.flags;
		val.u.p.orig_value = g_strdup(val.u.p.p);
		val.u.p.orig_flags = val.u.p.flags;
		break;
	case TYPE_FOLDER:
		if (value)
		{
			val.u.p.p = path_expand(value);
		}
		val.u.p.flags = flags;
		val.u.p.default_value = g_strdup(val.u.p.p);
		val.u.p.default_flags = val.u.p.flags;
		val.u.p.orig_value = g_strdup(val.u.p.p);
		val.u.p.orig_flags = val.u.p.flags;
		break;
	case TYPE_INT:
	case TYPE_UINT:
		if (value)
		{
			val.u.i.v = strtol(value, NULL, 0);
			val.u.i.default_value = val.u.i.v;
			val.u.i.orig_value = val.u.i.v;
		}
		val.u.i.minval = gtk_adjustment_get_lower(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(w)));
		val.u.i.maxval = gtk_adjustment_get_upper(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(w)));
		break;
	case TYPE_CHOICE:
		if (value)
		{
			val.u.c.c = strtol(value, NULL, 0);
			val.u.c.default_value = val.u.c.c;
			val.u.c.orig_value = val.u.c.c;
		}
		break;
	default:
		assert(0);
	}
	widget_set_pref_value(w, &val);
}

/*** ---------------------------------------------------------------------- ***/

static void enableTooltips(GuiWindow *gui)
{
	gboolean enable = gui->pref_show_tooltips && gtk_check_button_get_active(gui->pref_show_tooltips);
#if 0 /* much easier, but does not always work */
	GtkSettings *settings = gtk_settings_get_default();
	g_object_set(settings, "gtk-enable-tooltips", enable, NULL);
#else
	GtkWidget *w;
	GSList *l;
	const char *str;
	
	/* g_slist_foreach(gui->translation_list) */
	for (l = gui->translation_list; l; l = l->next)
	{
		w = l->data;
		assert(w);
		str = widget_get_tooltip(w);
		if (str)
		{
			gtk_widget_set_tooltip_text(w, enable ? _(str) : NULL);
		}
	}
	gtk_widget_set_tooltip_text(gui->ok_button, enable ? _("Accept and confirm the changes in this dialog") : NULL);
	gtk_widget_set_tooltip_text(gui->cancel_button, enable ? _("Discard the current changes and close the dialog") : NULL);
#endif
}

#ifdef ENABLE_NLS
static void retranslate_ui(GuiWindow *gui)
{
	GtkWidget *w;
	GSList *l;
	const char *str;
	
	/* g_slist_foreach(gui->translation_list) */
	for (l = gui->translation_list; l; l = l->next)
	{
		w = l->data;
		assert(w);
		str = widget_get_translation(w);
		if (str)
		{
			if (GTK_IS_LABEL(w))
			{
				gtk_label_set_text(GTK_LABEL(w), _(str));
			} else if (GTK_IS_BUTTON(w))
			{
				gtk_button_set_label(GTK_BUTTON(w), _(str));
			} else if (GTK_IS_WINDOW(w))
			{
				gtk_window_set_title(GTK_WINDOW(w), _(str));
			}
		}
		if (GTK_IS_COMBO_BOX(w))
		{
			GtkListStore *store;
			int i, count;
			GtkTreePath *path;
			GtkTreeIter iter;

			store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(w)));
			count = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL);
			for (i = 0; i < count; i++)
			{
				path = gtk_tree_path_new_from_indices(i, -1);
				if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path))
				{
					GValue value = G_VALUE_INIT;
					const char *str;
					
					gtk_tree_model_get_value(GTK_TREE_MODEL(store), &iter, CHOICE_COL_ORIG_TEXT, &value);
					str = g_value_get_string(&value);
					gtk_list_store_set(GTK_LIST_STORE(store), &iter, CHOICE_COL_TEXT, _(str), -1);
					g_value_unset(&value);
				}
				gtk_tree_path_free(path);
			}
		}
	}
	
	{
		GtkWidget *button;
		
		button = gtk_button_new_from_stock("gtk-ok");
		gtk_button_set_label(GTK_BUTTON(gui->ok_button), gtk_button_get_label(GTK_BUTTON(button)));
		gtk_widget_destroy(button);
		button = gtk_button_new_from_stock("gtk-cancel");
		gtk_button_set_label(GTK_BUTTON(gui->cancel_button), gtk_button_get_label(GTK_BUTTON(button)));
		gtk_widget_destroy(button);
	}

	enableTooltips(gui);
}

/*** ---------------------------------------------------------------------- ***/

static void cb_language_changed(GtkComboBox *widget, gpointer data)
{
	GuiWindow *gui = (GuiWindow *)data;
	int choice = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	language_t lang;
	const char *lang_name;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GValue value = G_VALUE_INIT;
	
	model = gtk_combo_box_get_model(GTK_COMBO_BOX(widget));
	path = gtk_tree_path_new_from_indices(choice, -1);
	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path))
	{
		assert(0);
	}
	gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, CHOICE_COL_VALUE, &value);
	gtk_tree_path_free(path);
	lang = (language_t)g_value_get_int(&value);
	g_value_unset(&value);
	if (lang == LANG_SYSTEM)
	{
		setlocale(LC_MESSAGES, "");
		lang = language_get_default();
	}
	{
		lang_name = language_get_name(lang);
		setlocale(LC_MESSAGES, lang_name);
	}
	retranslate_ui(gui);
}

#endif

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
	
	UNUSED(data);
	val = widget_get_pref_value(widget);
	assert(val);
#if GTK_CHECK_VERSION(4, 0, 0)
	{
		GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(widget));
		path = g_file_get_path(file);
		g_object_unref(G_OBJECT(file));
	}
#else
	path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));
#endif
	g_free(val->u.s.s);
	val->u.s.s = path;
}

/*** ---------------------------------------------------------------------- ***/

static void table_attach_span(GuiWindow *gui, GtkWidget *w, int row, int column, int height, int width)
{
#if GTK_CHECK_VERSION(4, 0, 0)
	gtk_grid_attach(GTK_GRID(gui->section_table), w, column, column + width, row, row + height);
#else
	gtk_table_attach(GTK_TABLE(gui->section_table), w, column, column + width, row, row + height, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
#endif
}

static void table_attach(GuiWindow *gui, GtkWidget *w, int row, int column)
{
	table_attach_span(gui, w, row, column, 1, 1);
}

/*
 * Called for opening tags like <foo bar="baz">
 */
static void parser_start_element(GMarkupParseContext *context, const char *element_name, const char **attribute_names, const char **attribute_values, gpointer user_data, GError **error)
{
	GuiWindow *gui = (GuiWindow *)user_data;
	gsize i;
	const char *name = 0;
	const char *icon_name = 0;
	const char *display = 0;
	const char *default_value = 0;
	const char *tooltip = 0;
	GtkWidget *settings_widget = 0;
	gboolean scrolled = FALSE;
	gboolean gui_only = FALSE;
	int type = TYPE_NONE;
	int flags = NO_FLAGS;

	UNUSED(context);
	UNUSED(error);

	for (i = 0; attribute_names[i]; i++)
	{
		if (strcmp(attribute_names[i], "name") == 0)
			name = attribute_values[i];
		else if (strcmp(attribute_names[i], "_label") == 0)
			display = attribute_values[i];
		else if (strcmp(attribute_names[i], "default") == 0)
			default_value = attribute_values[i];
		else if (strcmp(attribute_names[i], "_tooltip") == 0)
			tooltip = attribute_values[i];
		else if (strcmp(attribute_names[i], "scrolled") == 0)
			scrolled = TRUE;
		else if (strcmp(attribute_names[i], "icon") == 0)
			icon_name = attribute_values[i];
		else if (strcmp(attribute_names[i], "gui") == 0)
			gui_only = TRUE;
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
		GtkWidget *tab_widget;
		GtkWidget *label;

		assert(gui->section_table == NULL);
		assert(name != NULL);
		g_free(gui->section_name);
		gui->section_name = g_strdup(name);
		gui->section_names = g_slist_append(gui->section_names, g_strdup(name));
#if GTK_CHECK_VERSION(4, 0, 0)
		gui->section_table = gtk_grid_new();
#else
		gui->section_table = gtk_table_new(2, 2, FALSE);
		gtk_table_set_row_spacings(GTK_TABLE(gui->section_table), 4);
		gtk_table_set_col_spacings(GTK_TABLE(gui->section_table), 4);
#endif
		gtk_widget_show(gui->section_table);
		gui->section_row = 0;
		if (scrolled)
		{
			/*
			 * put the path list in a scrolled window, it gets too large
			 */
#if GTK_CHECK_VERSION(4, 0, 0)
			GtkWidget *scroller = gtk_scrolled_window_new();

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
			gtk_widget_show(scroller);
			tab_widget = scroller;
		} else
		{
			GtkWidget *vbox;

			vbox = gtk_vbox_new(FALSE, 0);
			gtk_widget_show(vbox);
			gtk_box_pack_start(GTK_BOX(vbox), gui->section_table, FALSE, FALSE, 0);
			tab_widget = vbox;
		}
		if (icon_name)
		{
#if 1
			GtkWidget *image = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_BUTTON);
			GtkWidget *hbox = gtk_hbox_new(0, 5);
			label = gtk_label_new(_(display));
			if (image)
			{
				gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, TRUE, 0);
				gtk_widget_show(image);
			}
			gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
			gtk_widget_show(hbox);
			gtk_notebook_append_page(GTK_NOTEBOOK(gui->notebook), tab_widget, hbox);
#else
			label = gtk_label_new(_(display));
			gtk_notebook_append_page(GTK_NOTEBOOK(gui->notebook), tab_widget, label);
#endif
		} else
		{
			label = gtk_label_new(_(display));
			gtk_notebook_append_page(GTK_NOTEBOOK(gui->notebook), tab_widget, label);
		}
		widget_set_translation(label, display);
		gui->translation_list = g_slist_append(gui->translation_list, label);
		gtk_widget_show(label);
	} else if (strcmp(element_name, "folder") == 0 ||
		strcmp(element_name, "path") == 0)
	{
		const char *title;
		GtkWidget *label;
		GtkWidget *button;

		assert(gui->section_table != NULL);
		label = gtk_label_new(_(display));
		gtk_widget_show(label);
		widget_set_translation(label, display);
		gui->translation_list = g_slist_append(gui->translation_list, label);
		type = strcmp(element_name, "folder") == 0 ? TYPE_FOLDER : TYPE_PATH;
		flags = NO_FLAGS;
		for (i = 0; attribute_names[i]; i++)
		{
			if (strcmp(attribute_names[i], "flags") == 0)
				flags = strtol(attribute_values[i], NULL, 0);
		}
		title = type == TYPE_FOLDER ? N_("Select a Folder") : N_("Select a File");
#if GTK_CHECK_VERSION(4, 0, 0)
		table_attach(gui, label, gui->section_row, 0);
		button = gtk_button_new_with_label(_(title));
		table_attach(gui, button, gui->section_row, 1);
		(void)cb_file_changed;
#else
		table_attach(gui, label, gui->section_row, 0);
		if (type == TYPE_FOLDER)
			button = gtk_file_chooser_button_new(_(title), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
		else
			button = gtk_file_chooser_button_new(_(title), GTK_FILE_CHOOSER_ACTION_OPEN);
		gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(button), TRUE);
		g_signal_connect(G_OBJECT(button), "file-set", G_CALLBACK(cb_file_changed), gui);
		table_attach(gui, button, gui->section_row, 1);
#endif
		widget_set_translation(button, title);
		widget_set_tooltip(button, tooltip);
		gui->translation_list = g_slist_append(gui->translation_list, button);
		gtk_widget_show(button);
		settings_widget = button;
		if (strcmp(name, "atari_drv_c") == 0 ||
			strcmp(name, "atari_drv_h") == 0 ||
			strcmp(name, "atari_drv_m") == 0 ||
			strcmp(name, "atari_drv_u") == 0)
			gtk_widget_set_sensitive(button, FALSE);
		if (flags != NO_FLAGS)
		{
			char *button_name;
			const char *button_text;
			GtkWidget *vbox;

			gui->section_row++;
			vbox = gtk_hbox_new(FALSE, 0);
			gtk_widget_show(vbox);
			table_attach_span(gui, vbox, gui->section_row, 1, 1, 2);
			
			button_name = g_strconcat(name, "_rdonly", NULL);
			button_text = N_("read-only");
			button = gtk_check_button_new_with_label(_(button_text));
			widget_set_translation(button, button_text);
			tooltip = N_("Mount drive in read-only mode, preventing writes from the emulation");
			widget_set_tooltip(button, tooltip);
			gui->translation_list = g_slist_append(gui->translation_list, button);
			gtk_widget_show(button);
			gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
			gtk_widget_set_name(button, button_name);
			g_object_set_data(G_OBJECT(settings_widget), button_name, button);
			
			button_name = g_strconcat(name, "_dosnames", NULL);
			button_text = N_("DOS 8+3 format");
			button = gtk_check_button_new_with_label(_(button_text));
			widget_set_translation(button, button_text);
			tooltip = N_("Force 8.3 short filenames (FAT-style) on host directories");
			widget_set_tooltip(button, tooltip);
			gui->translation_list = g_slist_append(gui->translation_list, button);
			gtk_widget_show(button);
			gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
			gtk_widget_set_name(button, button_name);
			g_object_set_data(G_OBJECT(settings_widget), button_name, button);
			
			button_name = g_strconcat(name, "_insensitive", NULL);
			button_text = N_("case insensitive");
			button = gtk_check_button_new_with_label(_(button_text));
			widget_set_translation(button, button_text);
			tooltip = N_("Ignore case sensitivity when accessing files/folders");
			widget_set_tooltip(button, tooltip);
			gui->translation_list = g_slist_append(gui->translation_list, button);
			gtk_widget_show(button);
			gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
			gtk_widget_set_name(button, button_name);
			g_object_set_data(G_OBJECT(settings_widget), button_name, button);
		}
		gui->section_row++;
	} else if (strcmp(element_name, "string") == 0)
	{
		GtkWidget *label;
		GtkWidget *entry;

		assert(gui->section_table != NULL);
		label = gtk_label_new(_(display));
		widget_set_translation(label, display);
		gui->translation_list = g_slist_append(gui->translation_list, label);
		gtk_widget_show(label);
		table_attach(gui, label, gui->section_row, 0);
		entry = gtk_entry_new();
		widget_set_tooltip(entry, tooltip);
		gui->translation_list = g_slist_append(gui->translation_list, entry);
		gtk_widget_show(entry);
		table_attach(gui, entry, gui->section_row, 1);
		settings_widget = entry;
		type = TYPE_STRING;
		gui->section_row++;
	} else if (strcmp(element_name, "int") == 0)
	{
		long minval = 0;
		long maxval = INT_MAX; /* LONG_MAX might not be representable in a double */
		long step = 1;
		GtkAdjustment *adjustment;
		GtkWidget *label;
		GtkWidget *button;
		
		assert(gui->section_table != NULL);
		label = gtk_label_new(_(display));
		widget_set_translation(label, display);
		gui->translation_list = g_slist_append(gui->translation_list, label);
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
		table_attach(gui, label, gui->section_row, 0);
		button = gtk_spin_button_new(GTK_ADJUSTMENT(adjustment), 0, 0);
		widget_set_tooltip(button, tooltip);
		gui->translation_list = g_slist_append(gui->translation_list, button);
		gtk_widget_show(button);
		gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(button), TRUE);
		table_attach(gui, button, gui->section_row, 1);
		settings_widget = button;
		type = TYPE_INT;
		/*
		 * some values must be written as unsigned, or the application fails to parse them
		 */
		if (strncmp(name, "app_window_", 11) == 0)
			type = TYPE_UINT;
		gui->section_row++;
	} else if (strcmp(element_name, "bool") == 0)
	{
		GtkWidget *button;

		assert(gui->section_table != NULL);
		button = gtk_check_button_new_with_label(_(display));
		widget_set_translation(button, display);
		widget_set_tooltip(button, tooltip);
		gui->translation_list = g_slist_append(gui->translation_list, button);
		gtk_widget_show(button);
		table_attach(gui, button, gui->section_row, 1);
		settings_widget = button;
		type = TYPE_BOOL;
		if (name != NULL && strcmp(name, "show_tooltips") == 0)
			gui->pref_show_tooltips = button;
		gui->section_row++;
	} else if (strcmp(element_name, "choice") == 0)
	{
		GtkWidget *label;
		GtkListStore *store;
		GtkCellRenderer *cell;

		assert(gui->section_table != NULL);
		assert(gui->combo_box == NULL);

		label = gtk_label_new(_(display));
		widget_set_translation(label, display);
		gui->translation_list = g_slist_append(gui->translation_list, label);
		gtk_widget_show(label);
		table_attach(gui, label, gui->section_row, 0);
		store = gtk_list_store_new(CHOICE_NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
		assert(store);
		cell = gtk_cell_renderer_text_new();
		assert(cell);
		gui->combo_box = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
		assert(gui->combo_box);
		gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(gui->combo_box), cell, TRUE);
		gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(gui->combo_box), cell, "text", CHOICE_COL_TEXT, NULL);
		widget_set_tooltip(gui->combo_box, tooltip);
		gui->translation_list = g_slist_append(gui->translation_list, gui->combo_box);
		gtk_widget_show(gui->combo_box);
		table_attach(gui, gui->combo_box, gui->section_row, 1);
		settings_widget = gui->combo_box;
		type = TYPE_CHOICE;
		gui->section_row++;
		if (strcmp(name, "gui_language") == 0)
		{
#ifdef ENABLE_NLS
			g_signal_connect(G_OBJECT(gui->combo_box), "changed", G_CALLBACK(cb_language_changed), gui);
#else
			gtk_widget_set_sensitive(gui->combo_box, FALSE);
#endif
		}
	} else if (strcmp(element_name, "select") == 0)
	{
		GtkListStore *store;
		GtkTreeIter iter;
		int value;

		assert(gui->section_table != NULL);
		assert(gui->combo_box != NULL);
		store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(gui->combo_box)));
		assert(GTK_IS_LIST_STORE(store));
		value = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL);
		for (i = 0; attribute_names[i]; i++)
		{
			if (strcmp(attribute_names[i], "value") == 0)
			{
				value = strtol(attribute_values[i], NULL, 0);
				break;
			}
		}
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, CHOICE_COL_TEXT, _(display), CHOICE_COL_ORIG_TEXT, display, CHOICE_COL_VALUE, value, -1);
	} else
	{
		/* FIXME: should be fatal error */
		g_printerr("unsupported element %s\n", element_name);
	}
	
	if (settings_widget)
	{
		if (name == NULL)
		{
			g_printerr("%s missing name\n", element_name);
		}
		widget_set_section(settings_widget, gui->section_name);
		gtk_widget_set_name(settings_widget, g_strdup(name));
		set_value(settings_widget, type, default_value, flags, gui_only);
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

	UNUSED(context);
	UNUSED(error);
	if (strcmp(element_name, "preferences") == 0)
	{
		assert(gui->section_table == NULL);
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
		assert(val);
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
	UNUSED(context);
	UNUSED(user_data);
	UNUSED(error);
	UNUSED(text);
	UNUSED(text_len);
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
	UNUSED(context);
	UNUSED(user_data);
	UNUSED(error);
	UNUSED(passthrough_text);
	UNUSED(text_len);
}

/*** ---------------------------------------------------------------------- ***/

/*
 * Called when any parsing method encounters an error. The GError should not be
 * freed.
 */
static void parser_error(GMarkupParseContext *context, GError *error, gpointer user_data)
{
	UNUSED(context);
	UNUSED(user_data);
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
		GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(gui->window),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CANCEL,
			_("can't create %s:\n%s"), gui->config_file, strerror(errno));
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		return FALSE;
	}
	/* g_slist_foreach(gui->widget_list) */
	for (l = gui->widget_list; l; l = l->next)
	{
		w = l->data;
		assert(w);
		val = widget_get_pref_value(w);
		assert(val);
		section = widget_get_section(w);
		name = gtk_widget_get_name(w);
		if (last_section == NULL || strcmp(last_section, section) != 0)
		{
			if (last_section != NULL)
				fputs("\n", f);
			fprintf(f, "[%s]\n", section);
			last_section = section;
			/* Cosmetic: write back original comment from example configuration */
			if (strcmp(section, "ADDITIONAL ATARI DRIVES") == 0)
			{
				fputs("# atari_drv_<A..T,V..Z> = flags [1:read-only, 2:8+3, 4:case-insensitive] path or image\n", f);
			}
		}
		if (val->gui_only)
		{
			/*
			 * mark as comment, so emulator does not choke on it
			 */
			fputs("#.", f);
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
			if (val->u.s.s != NULL && *val->u.s.s != '\0')
				fprintf(f, "%s = \"%s\"\n", name, val->u.s.s ? val->u.s.s : "");
			break;
		case TYPE_INT:
			fprintf(f, "%s = %ld\n", name, val->u.i.v);
			break;
		case TYPE_UINT:
			fprintf(f, "%s = %u\n", name, (unsigned int)val->u.i.v);
			break;
		case TYPE_BOOL:
			fprintf(f, "%s = %s\n", name, bool_to_string(val->u.b.b));
			break;
		case TYPE_CHOICE:
			fprintf(f, "%s = %d\n", name, val->u.c.c);
			/* Cosmetic: write back original comment from example configuration */
			if (strcmp(name, "atari_screen_colour_mode") == 0)
			{
				fputs("# 0:24b 1:16b 2:256 3:16 4:16ip 5:4ip 6:mono\n", f);
			}
			break;
		default:
			assert(0);
		}
	}
	fclose(f);
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
	while (*in_start != '\0' && *in_start != '\r' && *in_start != '\n' && *in_start != delimiter)
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

#define ISSPACE(c) ((c) == ' ' || (c) == '\t' || (c) == '\r' || (c) == '\n')

static gboolean evaluatePreferencesLine(GuiWindow *gui, const char *line, gboolean gui_only)
{
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
		GtkWidget *w = l->data;
		assert(w);
		val = widget_get_pref_value(w);
		assert(val);
		key = gtk_widget_get_name(w);
		keylen = strlen(key);
		if (g_ascii_strncasecmp(line, key, keylen) == 0 && (ISSPACE(line[keylen]) || line[keylen] == '='))
		{
			if (gui_only != val->gui_only)
			{
				g_printerr("%s: expected gui_only=%d, got %d\n", key, val->gui_only, gui_only);
				return FALSE;
			}
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
	while (ISSPACE(*line))
	{
		line++;
	}

	if (*line != '=')
	{
		return FALSE;
	}
	line++;

	/* skip spaces */
	while (ISSPACE(*line))
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
			while (ISSPACE(*line))
				line++;
		}
		str = eval_quotated_str_path(&line);
		if (str != NULL)
		{
			g_free(val->u.p.p);
			val->u.p.p = str;
			g_free(val->u.p.orig_value);
			val->u.p.orig_value = g_strdup(str);
			val->u.p.orig_flags = val->u.p.flags;
		} else
		{
			ok = FALSE;
		}
		break;

	case TYPE_STRING:
		str = eval_quotated_str(&line);
		if (str != NULL)
		{
			g_free(val->u.s.s);
			val->u.s.s = str;
			g_free(val->u.s.orig_value);
			val->u.s.orig_value = g_strdup(str);
		} else
		{
			ok = FALSE;
		}
		break;

	case TYPE_BOOL:
		ok &= eval_bool(&val->u.b.b, &line);
		if (ok)
			val->u.b.orig_value = val->u.b.b;
		break;

	case TYPE_INT:
	case TYPE_UINT:
		ok &= eval_int(&val->u.i.v, val->u.i.minval, val->u.i.maxval, &line);
		if (ok)
			val->u.i.orig_value = val->u.i.v;
		break;

	case TYPE_CHOICE:
		ok &= eval_int(&lv, 0, 0, &line);
		if (ok)
		{
			val->u.c.c = lv;
			val->u.c.orig_value = val->u.c.c;
		}
		break;

	default:
		assert(0);
	}

	if (ok)
	{
		/* skip trailing blanks */
		while (ISSPACE(*line))
		{
			line++;
		}
		if (*line != '\0')
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
	FILE *f;
	char line[2048];
	gboolean ok = TRUE;
	gboolean nline_ok;
	int lineno = 0;

	f = fopen(gui->config_file, "r");
	if (f == NULL)
	{
		/* Configuration file does not exist. Use defaults. */
		return TRUE;
	}

	while (fgets(line, sizeof(line) - 1, f))
	{
		const char *c = line;
		gboolean gui_only = FALSE;

		lineno++;

		/* skip spaces */
		while (ISSPACE(*c))
		{
			c++;
		}

		/* skip section names */
		if (*c == '[')
		{
			char *end;
			GSList *l;
			const char *section_name;

			c++;
			end = strchr(c, ']');
			if (end != NULL)
			{
				*end = '\0';
				for (l = gui->section_names; l; l = l->next)
				{
					assert(l);
					section_name = l->data;
					if (strcmp(section_name, c) == 0)
						break;
				}
				if (l == NULL)
				{
					/* FIXME: use dialog */
					g_printerr(_("%s:%d: unknown section '%s'\n"), gui->config_file, lineno, c);
				}
			}
			continue;
		}

		/* skip empty lines */
		if (*c == '\0')
		{
			continue;
		}

		/* skip comments */
		if (*c == '#')
		{
			if (c[1] != '.' || ISSPACE(c[2]))
			{
				/* TODO: stash it away so comments can be preserved */
				continue;
			}

			/*
			 * currently only for show_tooltips; maybe for others later
			 */
			c += 2;
			gui_only = TRUE;
		}

		nline_ok = evaluatePreferencesLine(gui, c, gui_only);
		if (!nline_ok)
		{
			/* FIXME: use dialog */
			g_printerr(_("%s:%d: Syntax error: %s"), gui->config_file, lineno, line);
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
		assert(w);
		val = widget_get_pref_value(w);
		assert(val);
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
			g_free(val->u.s.s);
			val->u.s.s = g_strdup(gtk_entry_get_text(GTK_ENTRY(w)));
			break;
		case TYPE_INT:
		case TYPE_UINT:
			val->u.i.v = gtk_spin_button_get_value(GTK_SPIN_BUTTON(w));
			break;
		case TYPE_BOOL:
			val->u.b.b = gtk_check_button_get_active(w);
			break;
		case TYPE_CHOICE:
			val->u.c.c = gtk_combo_box_get_active(GTK_COMBO_BOX(w));
			{
				GtkTreePath *path;
				GtkTreeIter iter;
				GtkListStore *model;
				GValue value = G_VALUE_INIT;
				
				model = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(w)));
				path = gtk_tree_path_new_from_indices(val->u.c.c, -1);
				if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path))
				{
					assert(0);
				}
				gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, CHOICE_COL_VALUE, &value);
				gtk_tree_path_free(path);
				val->u.c.c = g_value_get_int(&value);
				g_value_unset(&value);
			}
			break;
		default:
			assert(0);
		}
	}
	return TRUE;
}

static void cb_tooltips(GtkWidget *widget, gpointer data)
{
	UNUSED(widget);
	enableTooltips((GuiWindow *)data);
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
		assert(w);
		val = widget_get_pref_value(w);
		assert(val);
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
			if (val->u.s.s)
			{
				gtk_entry_set_text(GTK_ENTRY(w), val->u.s.s);
			}
			break;
		case TYPE_INT:
		case TYPE_UINT:
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), val->u.i.v);
			break;
		case TYPE_BOOL:
			gtk_check_button_set_active(w, val->u.b.b);
			break;
		case TYPE_CHOICE:
			{
				int i;
				GtkTreePath *path;
				GtkTreeIter iter;
				GtkListStore *model;
				
				/*
				 * convert the value from preferences to the combobox index
				 */
				model = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(w)));
				for (i = 0; i <= val->u.c.maxval; i++)
				{
					path = gtk_tree_path_new_from_indices(i, -1);
					if (gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path))
					{
						GValue value = G_VALUE_INIT;
						int v;
						
						gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, CHOICE_COL_VALUE, &value);
						v = g_value_get_int(&value);
						g_value_unset(&value);
						if (v == val->u.c.c)
						{
							val->u.c.c = i;
#if 0 /* not here: updatePreferences has already converted the value again */
							val->u.c.orig_value = i;
#endif
							gtk_tree_path_free(path);
							break;
						}
					}
					gtk_tree_path_free(path);
				}
			}
			gtk_combo_box_set_active(GTK_COMBO_BOX(w), val->u.c.c);
			break;
		default:
			assert(0);
		}
	}
	enableTooltips(gui);
	if (gui->pref_show_tooltips)
		g_signal_connect(G_OBJECT(gui->pref_show_tooltips), "toggled", G_CALLBACK(cb_tooltips), gui);
	return TRUE;
}


static gboolean strnull_equal(const char *s1, const char *s2)
{
	if (s1 == NULL || *s1 == '\0')
		return s2 == NULL || *s2 == '\0';
	if (s2 == NULL || *s2 == '\0')
		return s1 == NULL || *s1 == '\0';
	return strcmp(s1, s2) == 0;
}

static gboolean anyChanged(GuiWindow *gui)
{
	GtkWidget *w;
	GSList *l;
	struct pref_val *val;
	gboolean changed = FALSE;
	gboolean this_changed;
	
	/* g_slist_foreach(gui->widget_list) */
	for (l = gui->widget_list; l; l = l->next)
	{
		w = l->data;
		assert(w);
		val = widget_get_pref_value(w);
		assert(val);
		switch (val->type)
		{
		case TYPE_NONE:
			this_changed = FALSE;
			break;
		case TYPE_PATH:
		case TYPE_FOLDER:
			this_changed = !strnull_equal(val->u.p.p, val->u.p.orig_value) || val->u.p.flags != val->u.p.orig_flags;
			break;
		case TYPE_STRING:
			this_changed = !strnull_equal(val->u.s.s, val->u.s.orig_value);
			break;
		case TYPE_INT:
		case TYPE_UINT:
			this_changed = val->u.i.v != val->u.i.orig_value;
			break;
		case TYPE_BOOL:
			this_changed = val->u.b.b != val->u.b.orig_value;
			break;
		case TYPE_CHOICE:
			this_changed = val->u.c.c != val->u.c.orig_value;
			break;
		default:
			assert(0);
		}
		changed |= this_changed;
	}
	return changed;
}

/*** ---------------------------------------------------------------------- ***/

static void mxgtk_quit(GuiWindow *gui)
{
#if GTK_CHECK_VERSION(4, 0, 0)
	exit(gui->exit_code);
#else
	UNUSED(gui);
	gtk_main_quit();
#endif
}

/*** ---------------------------------------------------------------------- ***/

static void send_delete_event(GtkWidget *widget)
{
	GdkEvent *event = gdk_event_new(GDK_DELETE);

	event->any.window = g_object_ref(gtk_widget_get_window(widget));
	event->any.send_event = FALSE;

	g_object_ref(widget);

	if (!gtk_widget_event(widget, event))
		gtk_widget_destroy(widget);

	g_object_unref(widget);

	gdk_event_free(event);
}

#if GTK_CHECK_VERSION(4, 0, 0)
static gboolean cb_key_press(GtkWidget *widget, guint keyval, guint keycode, GdkModifierType state, GtkEventController *event_controller)
{
	UNUSED(widget);
	UNUSED(keycode);
	UNUSED(state);
	UNUSED(event_controller);
	if (keyval == GDK_KEY_Escape)
	{
		GuiWindow *gui = g_object_get_data(G_OBJECT(widget), "gui");
		send_delete_event(gui->window);
	}
	return FALSE;
}
#else
static gboolean cb_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	GuiWindow *gui = (GuiWindow *)data;

	UNUSED(widget);
	if (event->keyval == GDK_KEY_Escape)
	{
		send_delete_event(gui->window);
	}
	return FALSE;
}
#endif

/*** ---------------------------------------------------------------------- ***/

static gboolean cb_deleted(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	GuiWindow *gui = (GuiWindow *)data;
	
	UNUSED(event);
	UNUSED(widget);
	updatePreferences(gui);
	if (anyChanged(gui))
	{
		int response;
		GtkWidget *dialog = gtk_dialog_new_with_buttons(gtk_window_get_title(GTK_WINDOW(gui->window)), GTK_WINDOW(gui->window),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			GTK_STOCK_DELETE, GTK_RESPONSE_REJECT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CLOSE,
			NULL);
		GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
		GtkWidget *label = gtk_label_new(_("You have unsaved changes. Do you want to save them and quit?"));

		gtk_widget_show(label);
		gtk_container_add(GTK_CONTAINER(content), label);
		response = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		switch (response)
		{
		case GTK_RESPONSE_ACCEPT:
			if (!writePreferences(gui))
				return TRUE;
			break;
		case GTK_RESPONSE_REJECT:
			break;
		case GTK_RESPONSE_DELETE_EVENT:
			return TRUE;
		case GTK_RESPONSE_CLOSE:
		default:
			return TRUE;
		}
	}
	mxgtk_quit(gui);
	return FALSE;
}

/*** ---------------------------------------------------------------------- ***/

static void cb_ok(GtkWidget *widget, gpointer data)
{
	GuiWindow *gui = (GuiWindow *)data;

	UNUSED(widget);
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

	UNUSED(widget);
	updatePreferences(gui);
	if (anyChanged(gui))
	{
		int response;
		GtkWidget *dialog = gtk_dialog_new_with_buttons(gtk_window_get_title(GTK_WINDOW(gui->window)), GTK_WINDOW(gui->window),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			GTK_STOCK_DELETE, GTK_RESPONSE_REJECT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CLOSE,
			NULL);
		GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
		GtkWidget *label = gtk_label_new(_("You have unsaved changes. Do you want to discard them and quit?"));

		gtk_widget_show(label);
		gtk_container_add(GTK_CONTAINER(content), label);
		response = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		switch (response)
		{
		case GTK_RESPONSE_ACCEPT:
			if (!writePreferences(gui))
				return;
			break;
		case GTK_RESPONSE_REJECT:
			break;
		case GTK_RESPONSE_DELETE_EVENT:
			return;
		case GTK_RESPONSE_CLOSE:
		default:
			return;
		}
	}
	gui->exit_code = EXIT_FAILURE;
	mxgtk_quit(gui);
}

/*** ---------------------------------------------------------------------- ***/

static void cb_window_destroy(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GuiWindow *gui = (GuiWindow *)data;
	UNUSED(widget);
	UNUSED(event);
	mxgtk_quit(gui);
}

/*** ---------------------------------------------------------------------- ***/

static void window_create(GuiWindow *gui)
{
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GtkWidget *hbox;
	GtkWidget *btn_box;
	const char *title;

	/* g_object_set(gtk_settings_get_default(), "gtk-button-images", TRUE, NULL); */

	gtk_window_set_default_icon_name(program_name);

#if GTK_CHECK_VERSION(4, 0, 0)
	gui->window = gtk_window_new();
#else
	gui->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_accept_focus(GTK_WINDOW(gui->window), TRUE);
	gtk_container_set_border_width(GTK_CONTAINER(gui->window), 12);
#endif
	g_object_set_data(G_OBJECT(gui->window), "gui", gui);
	g_signal_connect(G_OBJECT(gui->window), "destroy", G_CALLBACK(cb_window_destroy), gui);
#if GTK_CHECK_VERSION(4, 0, 0)
	{
		GtkEventController *event_controller = gtk_event_controller_key_new();
		g_signal_connect_object(event_controller, "key-pressed", G_CALLBACK(cb_key_press), G_OBJECT(gui->window), G_CONNECT_SWAPPED);
		gtk_widget_add_controller(GTK_WIDGET(gui->window), event_controller);
	}
#else
	g_signal_connect(G_OBJECT(gui->window), "key_press_event", G_CALLBACK(cb_key_press), gui);
#endif
	g_signal_connect(G_OBJECT(gui->window), "delete-event", G_CALLBACK(cb_deleted), gui);

	title = N_("MagicOnLinux Settings");
	gtk_window_set_title(GTK_WINDOW(gui->window), _(title));
	widget_set_translation(gui->window, title);
	gui->translation_list = g_slist_append(gui->translation_list, gui->window);
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
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(gui->notebook), TRUE);
	gtk_widget_show(gui->notebook);
#if !GTK_CHECK_VERSION(4, 0, 0)
	gtk_container_set_border_width(GTK_CONTAINER(gui->notebook), 6);
#endif
	gtk_box_pack_start(GTK_BOX(hbox), gui->notebook, TRUE, TRUE, 0);
	
#if GTK_CHECK_VERSION(4, 0, 0)
	btn_box = gtk_hbox_new(FALSE, 0);
#else
	btn_box = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(btn_box), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(btn_box), 6);
#endif
	gtk_box_pack_end(GTK_BOX(vbox), btn_box, FALSE, FALSE, 0);
	gtk_widget_show(btn_box);

	gui->cancel_button = gtk_button_new_from_stock("gtk-cancel");
	gtk_widget_show(gui->cancel_button);
	g_signal_connect(G_OBJECT(gui->cancel_button), "clicked", G_CALLBACK(cb_cancel), gui);
	gtk_box_pack_start(GTK_BOX(btn_box), gui->cancel_button, FALSE, FALSE, 0);

	gui->ok_button = gtk_button_new_from_stock("gtk-ok");
	gtk_widget_show(gui->ok_button);
	g_signal_connect(G_OBJECT(gui->ok_button), "clicked", G_CALLBACK(cb_ok), gui);
	gtk_box_pack_start(GTK_BOX(btn_box), gui->ok_button, FALSE, FALSE, 0);
	gtk_widget_grab_focus(gui->ok_button);

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

static char *geom_arg;
static gboolean bShowVersion;
static gboolean bShowHelp;
static const char *config_file_arg;

static GOptionEntry options[] = {
	{ "geometry", 0, 0, G_OPTION_ARG_STRING, &geom_arg, N_("Sets the client geometry of the main window"), N_("GEOMETRY") },
	{ "config", 0, 0, G_OPTION_ARG_STRING, &config_file_arg, N_("Specify an alternative configuration file path"), N_("FILE") },
	{ "version", 0, 0, G_OPTION_ARG_NONE, &bShowVersion, N_("Show version information and exit"), NULL },
	{ "help", '?', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &bShowHelp, N_("Show help information and exit"), NULL },

	{ NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
};

static gboolean ParseCommandLine(int *argc, char ***argv)
{
	GOptionContext *context;
	GOptionGroup *gtk_group;
	GError *error = NULL;
	gboolean retval;
	int i;
	GOptionGroup *main_group;
	
	/*
	 * Glib's option parser requires global variables,
	 * copy options read from INI file there.
	 */
	bShowVersion = FALSE;
	bShowHelp = FALSE;
	
#if GTK_CHECK_VERSION(4, 0, 0)
	gtk_group = g_option_group_new("main", "main", "main", NULL, NULL);
#else
	gtk_group = gtk_get_option_group(FALSE);
#endif
	context = g_option_context_new("");
	main_group = g_option_group_new(NULL, NULL, NULL, NULL, NULL);
	g_option_context_set_main_group(context, main_group);
	g_option_context_set_summary(context, _("GTK Configurator for MagicOnLinux"));
	g_option_context_add_group(context, gtk_group);
	/*
	 * we must lookup the translations of the option texts ourselfes,
	 * because glib would use libintl, not our functions
	 */
	for (i = 0; options[i].long_name != NULL; i++)
	{
		options[i].description = _(options[i].description);
		options[i].arg_description = _(options[i].arg_description);
	}
	
	g_option_context_add_main_entries(context, options, NULL);

	g_option_context_set_help_enabled(context, FALSE);
	
	retval = g_option_context_parse(context, argc, argv, &error);
	if (bShowHelp)
	{
		char *msg = g_option_context_get_help(context, FALSE, NULL);
		fprintf(stdout, "%s\n", msg);
		g_free(msg);
	}
	g_option_context_free(context);
	
	if (retval == FALSE)
	{
		char *msg = g_strdup_printf("%s: %s", program_name, error && error->message ? error->message : _("error parsing command line"));
		fprintf(stdout, "%s\n", msg);
		g_free(msg);
		g_clear_error(&error);
		return FALSE;
	}
	
	return TRUE;
}

/*** ---------------------------------------------------------------------- ***/

static void show_version(void)
{
	printf("%s %s\n", program_name, program_version);
}

/*** ---------------------------------------------------------------------- ***/

int main(int argc, char **argv)
{
	int i;
	gboolean ok;
	GMarkupParseContext *context;
	GuiWindow gui;
	
	for (i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-session") == 0 || strcmp(argv[i], "--session") == 0)
			return EXIT_SUCCESS;
	}

	setlocale(LC_ALL, "");

#ifdef ENABLE_NLS
	{
		const char *lang_name;
		lang_name = language_get_name(language_get_default());
#ifdef FORCE_LIBINTL
		bindtextdomain(_STRINGIFY(GETTEXT_PACKAGE), PACKAGE_LOCALE_DIR);
#else
		bindtextdomain(_STRINGIFY(GETTEXT_PACKAGE), NULL);
#endif
		textdomain(_STRINGIFY(GETTEXT_PACKAGE));
		bind_textdomain_codeset(_STRINGIFY(GETTEXT_PACKAGE), "UTF-8");
		setlocale(LC_MESSAGES, lang_name);
	}
#endif

	/* workaround for org.gtk.vfs.GoaVolumeMonitor sometimes hanging */
	unsetenv("DBUS_SESSION_BUS_ADDRESS");

	if (!ParseCommandLine(&argc, &argv))
		return EXIT_FAILURE;
	
	if (bShowHelp)
	{
		/* help already shown */
		return EXIT_SUCCESS;
	}
	if (bShowVersion)
	{
		show_version();
		return EXIT_SUCCESS;
	}
	
#if GTK_CHECK_VERSION(4, 0, 0)
	gtk_init();
#else
	ok = gtk_init_check(&argc, &argv);
	if (!ok)
	{
		g_printerr("%s: unable to initialize GTK\n", program_name);
		return EXIT_FAILURE;
	}
#endif

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

	if (config_file_arg != NULL)
	{
		gui.config_file = path_expand(config_file_arg);
	} else
	{
#if defined(__APPLE__)
		gui.config_file = path_expand("~/Library/Preferences/magiclinux.conf");
#else
		gui.config_file = path_expand("~/.config/magiclinux.conf");
#endif
	}
	getPreferences(&gui);
	populatePreferences(&gui);
#ifdef ENABLE_NLS
	retranslate_ui(&gui);
#endif

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
