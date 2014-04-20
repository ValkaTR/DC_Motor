// #############################################################################

#include "util.h"

// #############################################################################

gchar *mousepad_util_get_save_location (const gchar *relpath, gboolean create_parents)
{
  gchar *filename, *dirname;

  g_return_val_if_fail (g_get_user_config_dir () != NULL, NULL);

  /* create the full filename */
  filename = g_build_filename (g_get_user_config_dir (), relpath, NULL);

  /* test if the file exists */
  if (G_UNLIKELY (g_file_test (filename, G_FILE_TEST_EXISTS) == FALSE))
    {
      if (create_parents)
        {
          /* get the directory name */
          dirname = g_path_get_dirname (filename);

          /* make the directory with parents */
          if (g_mkdir_with_parents (dirname, 0700) == -1)
            {
              /* show warning to the user */
              g_critical (_("Unable to create base directory \"%s\". "
                            "Saving to file \"%s\" will be aborted."), dirname, filename);

              /* don't return a filename, to avoid problems */
              g_free (filename);
              filename = NULL;
            }

          /* cleanup */
          g_free (dirname);
        }
      else
        {
          /* cleanup */
          g_free (filename);
          filename = NULL;
        }
    }

  return filename;
}

// #############################################################################

void mousepad_util_save_key_file (GKeyFile *keyfile, const gchar *filename)
{
  gchar  *contents;
  gsize   length;
  GError *error = NULL;

  /* get the contents of the key file */
  contents = g_key_file_to_data (keyfile, &length, &error);

  if (G_LIKELY (error == NULL))
    {
      /* write the contents to the file */
      if (G_UNLIKELY (g_file_set_contents (filename, contents, length, &error) == FALSE))
        goto print_error;
    }
  else
    {
      print_error:

      /* print error */
      g_critical (_("Failed to store the preferences to \"%s\": %s"), filename, error->message);

      /* cleanup */
      g_error_free (error);
    }

  /* cleanup */
  g_free (contents);
}

// #############################################################################
