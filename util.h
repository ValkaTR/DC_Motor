// #############################################################################

#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <math.h>

// #############################################################################

#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

// #############################################################################

//#define _(STRING) gettext(STRING)
#define _(STRING) STRING

gchar *mousepad_util_get_save_location (const gchar *relpath, gboolean create_parents);
void mousepad_util_save_key_file (GKeyFile *keyfile, const gchar *filename);

// #############################################################################

#endif // UTIL_H_INCLUDED

// #############################################################################
