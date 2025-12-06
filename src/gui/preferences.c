#include "preferences.h"

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

