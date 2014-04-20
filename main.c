// #############################################################################

#include <unistd.h> // exec

#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <math.h>

#include <gsl/gsl_math.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_odeiv2.h>

#include <plplot.h>

#include "main.h"
#include "util.h"
#include "plot.h"
#include "DC_motor.h"

// #############################################################################

const gchar *workspace_filename = "DC_motor.glade";
const gchar *applicationwindow_name = "applicationwindow1";

// #############################################################################

extern enum LOAD_TYPE load_type;
extern double temperature;
extern double electrical_resistivity;
extern double Ra;
extern double La;
extern double Jsum;
extern double Pn;
extern double Nn;
extern double Un;
extern double kmPhi;

extern double load_torque;

extern double Imin;
extern double Imax;

extern double current_kp;
extern double current_ki;
extern double current_kd;
extern double current_error_sum_max;
extern double current_error_sum_min;

extern double omega_kp;
extern double omega_ki;
extern double omega_kd;
extern double omega_error_sum_max;
extern double omega_error_sum_min;

gint combobox_motor_resistance_grade = 0;
gint combobox_motor_inductance_grade = 0;
gint combobox_motor_power_grade = 0;

extern enum REGULATOR_STATE regulator_state;
extern enum REGULATOR_MODE regulator_mode;

// #############################################################################

G_MODULE_EXPORT void starte_dialog( GtkWidget *widget, gpointer *user_data )
{
    GtkWidget *dialog = GTK_WIDGET( user_data );

    gtk_widget_show_all( dialog );
    //g_signal_connect_swapped( dialog, "response",
    //    G_CALLBACK( gtk_widget_hide ), dialog );
}

G_MODULE_EXPORT void on_settings_response( GtkWidget *dialog, gint response_id, struct CallbackObjekt *obj )
{
    if( response_id == GTK_RESPONSE_OK )
    {
        // Save settings
        //messagebox(obj->builder, "save");

        settings_update( obj );
        settings_store( );

        gtk_widget_hide( dialog );
    }
    else if( response_id == GTK_RESPONSE_APPLY )
    {
        //messagebox(obj->builder, "save");
        settings_update( obj );
        settings_store( );
    }
    else if( response_id == GTK_RESPONSE_CANCEL )
    {
        // Do nothing
        gtk_widget_hide( dialog );
    }

}

G_MODULE_EXPORT void on_settings_show( GtkWidget *dialog, struct CallbackObjekt *obj )
{
    settings_init( obj );
}

G_MODULE_EXPORT void on_menuitem_preferences_activate( GtkWidget *menuitem, struct CallbackObjekt *obj )
{
    //GtkWidget *dialog = GTK_WIDGET( gtk_builder_get_object( obj->builder, "settings" ) );

    //gint response = gtk_dialog_run( GTK_DIALOG(dialog) );

    //messagebox(obj->builder, "asdfasfdsad");
}

// #############################################################################

void spin_button_set_double( struct CallbackObjekt *obj, gchar *spin_button_name, gdouble spin_button_value )
{
    GtkWidget *spin_button = GTK_WIDGET( gtk_builder_get_object( obj->builder, spin_button_name ) );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( spin_button ), spin_button_value );
}

void entry_set_double( struct CallbackObjekt *obj, gchar *entry_name, gdouble entry_value )
{
    GtkWidget *entry = GTK_WIDGET( gtk_builder_get_object( obj->builder, entry_name ) );
    gchar *buffer = g_malloc( G_ASCII_DTOSTR_BUF_SIZE );
    g_ascii_dtostr( buffer, G_ASCII_DTOSTR_BUF_SIZE, entry_value );
    gtk_entry_set_text( GTK_ENTRY( entry ), buffer );
    g_free( buffer );
}

void entry_set_double_with_grade( struct CallbackObjekt *obj, gchar *entry_name, gchar *combobox_name, gdouble entry_value, gint *combobox_grade )
{
    entry_set_double( obj, entry_name, entry_value * pow( 10, - *combobox_grade ) );

    GtkWidget *combobox = GTK_WIDGET( gtk_builder_get_object( obj->builder, combobox_name ) );
    GtkTreeModel *combobox_model = gtk_combo_box_get_model( GTK_COMBO_BOX( combobox ) );

    // Get the first iter in the list, check it is valid and walk
    //through the list, reading each row.
    GtkTreeIter combobox_iter;
    for( gboolean valid = gtk_tree_model_get_iter_first( combobox_model, &combobox_iter );
         valid;
         valid = gtk_tree_model_iter_next( combobox_model, &combobox_iter )
    )
    {
       gint int_data;

       /* Make sure you terminate calls to gtk_tree_model_get()
        * with a '-1' value
        */
       gtk_tree_model_get(combobox_model, &combobox_iter,
           0, &int_data,
           -1
       );

       /* Do something with the data */
       if( *combobox_grade == int_data )
       {
           gtk_combo_box_set_active_iter( GTK_COMBO_BOX( combobox ), &combobox_iter );
           break;
       }
    }
}

void checkbox_set_boolean( struct CallbackObjekt *obj, gchar *checkbox_name, gdouble checkbox_value )
{
    GtkWidget *checkbutton = GTK_WIDGET( gtk_builder_get_object( obj->builder, checkbox_name ) );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( checkbutton ), checkbox_value );
}

// Initialize UI states in dialog from variables
gboolean settings_init( struct CallbackObjekt *obj )
{
    GtkWidget *combo_load_type = GTK_WIDGET( gtk_builder_get_object( obj->builder, "combobox_load_type" ) );
    gtk_combo_box_set_active( GTK_COMBO_BOX(combo_load_type), load_type );

    entry_set_double( obj, "entry_supply_voltage", Un );
    entry_set_double( obj, "entry_minimum_armature_current", Imin );

    spin_button_set_double( obj, "spinbutton_current_p", current_kp );
    spin_button_set_double( obj, "spinbutton_current_i", current_ki );
    spin_button_set_double( obj, "spinbutton_current_d", current_kd );
    spin_button_set_double( obj, "spinbutton_speed_p", omega_kp );
    spin_button_set_double( obj, "spinbutton_speed_i", omega_ki );
    spin_button_set_double( obj, "spinbutton_speed_d", omega_kd );

    entry_set_double( obj, "entry_motor_inertia", Jsum );
    entry_set_double( obj, "entry_at_temp", temperature );
    entry_set_double( obj, "entry_electrical_resistivity", electrical_resistivity );
    //entry_set_double( obj, "entry_motor_torque", electrical_resistivity );
    entry_set_double( obj, "entry_kmphi", kmPhi );
    entry_set_double_with_grade( obj, "entry_motor_resistance", "combobox_motor_resistance", Ra, &combobox_motor_resistance_grade );
    entry_set_double_with_grade( obj, "entry_motor_inductance", "combobox_motor_inductance", La, &combobox_motor_inductance_grade );
    entry_set_double_with_grade( obj, "entry_motor_power", "combobox_motor_power", Pn, &combobox_motor_power_grade );

    entry_set_double( obj, "entry_load_torque", load_torque );

    checkbox_set_boolean( obj, "checkbutton_pid_controller", ( regulator_mode == REGULATOR_MODE_SPEED ? TRUE : FALSE ) );

    return TRUE;
}

void checkbox_get_boolean( struct CallbackObjekt *obj, gchar *checkbox_name, gboolean *checkbox_value )
{
    GtkWidget *checkbutton = GTK_WIDGET( gtk_builder_get_object( obj->builder, checkbox_name ) );
    *checkbox_value = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( checkbutton ) );
}

void spin_button_get_double( struct CallbackObjekt *obj, gchar *spin_button_name, gdouble *spin_button_value )
{
    GtkWidget *spin_button = GTK_WIDGET( gtk_builder_get_object( obj->builder, spin_button_name ) );
    *spin_button_value = gtk_spin_button_get_value( GTK_SPIN_BUTTON( spin_button ) );
}

void entry_get_double( struct CallbackObjekt *obj, gchar *entry_name, gdouble *entry_value )
{
    GtkWidget *entry = GTK_WIDGET( gtk_builder_get_object( obj->builder, entry_name ) );
    const gchar *buffer = gtk_entry_get_text( GTK_ENTRY( entry ) );
    *entry_value = g_ascii_strtod( buffer, NULL );
}

void entry_get_double_with_grade( struct CallbackObjekt *obj, gchar *entry_name, gchar *combobox_name, gdouble *entry_value, gint *combobox_grade )
{
    GtkWidget *combobox = GTK_WIDGET( gtk_builder_get_object( obj->builder, combobox_name ) );
    GtkTreeIter combobox_iter;
    if( gtk_combo_box_get_active_iter( GTK_COMBO_BOX( combobox ), &combobox_iter ) )
    {
        GtkTreeModel *combobox_model = gtk_combo_box_get_model( GTK_COMBO_BOX( combobox ) );
        gtk_tree_model_get( combobox_model, &combobox_iter,
            0, combobox_grade,
            -1
        );
    }

    gdouble temp;
    entry_get_double( obj, entry_name, &temp );
    *entry_value = temp * pow( 10, *combobox_grade );
}

// Get UI states from the dialog and store in variables
gboolean settings_update( struct CallbackObjekt *obj )
{
    GtkWidget *combo_load_type = GTK_WIDGET( gtk_builder_get_object( obj->builder, "combobox_load_type" ) );
    load_type = gtk_combo_box_get_active( GTK_COMBO_BOX(combo_load_type) );

    entry_get_double( obj, "entry_supply_voltage", &Un );
    entry_get_double( obj, "entry_minimum_armature_current", &Imin );

    spin_button_get_double( obj, "spinbutton_current_p", &current_kp );
    spin_button_get_double( obj, "spinbutton_current_i", &current_ki );
    spin_button_get_double( obj, "spinbutton_current_d", &current_kd );
    spin_button_get_double( obj, "spinbutton_speed_p", &omega_kp );
    spin_button_get_double( obj, "spinbutton_speed_i", &omega_ki );
    spin_button_get_double( obj, "spinbutton_speed_d", &omega_kd );

    entry_get_double( obj, "entry_motor_inertia", &Jsum );
    entry_get_double( obj, "entry_at_temp", &temperature );
    entry_get_double( obj, "entry_electrical_resistivity", &electrical_resistivity );
    //entry_set_double( obj, "entry_motor_torque", electrical_resistivity );
    entry_get_double( obj, "entry_kmphi", &kmPhi );
    entry_get_double_with_grade( obj, "entry_motor_resistance", "combobox_motor_resistance", &Ra, &combobox_motor_resistance_grade );
    entry_get_double_with_grade( obj, "entry_motor_inductance", "combobox_motor_inductance", &La, &combobox_motor_inductance_grade );
    entry_get_double_with_grade( obj, "entry_motor_power", "combobox_motor_power", &Pn, &combobox_motor_power_grade );

    entry_get_double( obj, "entry_load_torque", &load_torque );

    gboolean checkbutton_active;
    checkbox_get_boolean( obj, "checkbutton_pid_controller", &checkbutton_active );
    regulator_mode = ( checkbutton_active == TRUE ? REGULATOR_STATE_SPEED : REGULATOR_STATE_NONE );

    return TRUE;
}

// Load variables from the file
gboolean settings_load( )
{
    /* get the save location, leave when there if no file found */
    gchar *filename = mousepad_util_get_save_location (DC_MOTOR_RC_RELPATH, FALSE);
    if (G_UNLIKELY (filename == NULL))
        return FALSE;

    /* create a new key file */
    GKeyFile *keyfile = g_key_file_new ();

    /* open the config file */
    if (G_LIKELY (g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, NULL)))
    {
        /* read the propert value from the key file */
        Un = g_key_file_get_double( keyfile, "Electrical", "voltage", NULL );
        Imin = g_key_file_get_double( keyfile, "Electrical", "minimum_armature_current", NULL );
        gdouble pid = g_key_file_get_boolean( keyfile, "Electrical", "pid", NULL );
        regulator_mode = (pid == TRUE ? REGULATOR_MODE_SPEED : REGULATOR_MODE_NONE);

        current_kp = g_key_file_get_double( keyfile, "Electrical", "current_kp", NULL );
        current_ki = g_key_file_get_double( keyfile, "Electrical", "current_ki", NULL );
        current_kd = g_key_file_get_double( keyfile, "Electrical", "current_kd", NULL );
        omega_kp = g_key_file_get_double( keyfile, "Electrical", "omega_kp", NULL );
        omega_ki = g_key_file_get_double( keyfile, "Electrical", "omega_ki", NULL );
        omega_kd = g_key_file_get_double( keyfile, "Electrical", "omega_kd", NULL );

        Ra = g_key_file_get_double( keyfile, "Motor", "armature_resistance", NULL );
        La = g_key_file_get_double( keyfile, "Motor", "armature_inductance", NULL );
        Jsum = g_key_file_get_double( keyfile, "Motor", "inertia", NULL );
        Pn = g_key_file_get_double( keyfile, "Motor", "power", NULL );
        Nn = g_key_file_get_double( keyfile, "Motor", "speed", NULL );
        kmPhi = g_key_file_get_double( keyfile, "Motor", "speed_constant", NULL );
        combobox_motor_resistance_grade = g_key_file_get_integer( keyfile, "Motor", "armature_resistance_grade", NULL );
        combobox_motor_inductance_grade = g_key_file_get_integer( keyfile, "Motor", "armature_inductance_grade", NULL );
        combobox_motor_power_grade = g_key_file_get_integer( keyfile, "Motor", "armature_power_grade", NULL );

        load_type = g_key_file_get_integer( keyfile, "Load", "type", NULL );
    }

    /* free the key file */
    g_key_file_free (keyfile);

    /* cleanup filename */
    g_free (filename);

    return TRUE;
}


// Store variables to the file
gboolean settings_store( )
{
    /* get the config filename */
    gchar *filename = mousepad_util_get_save_location (DC_MOTOR_RC_RELPATH, TRUE);

    /* leave when no filename is returned */
    if (G_UNLIKELY (filename == NULL))
        return FALSE;

    /* create an empty key file */
    GKeyFile *keyfile = g_key_file_new ();

    /* try to load the file contents, no worries if this fails */
    g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, NULL);


    g_key_file_set_double( keyfile, "Electrical", "voltage", Un );
    g_key_file_set_double( keyfile, "Electrical", "minimum_armature_current", Imin );

    g_key_file_set_boolean( keyfile, "Electrical", "pid", (regulator_mode == REGULATOR_MODE_SPEED  ? TRUE : FALSE) );
    g_key_file_set_double( keyfile, "Electrical", "current_kp", current_kp );
    g_key_file_set_double( keyfile, "Electrical", "current_ki", current_ki );
    g_key_file_set_double( keyfile, "Electrical", "current_kd", current_kd );
    g_key_file_set_double( keyfile, "Electrical", "omega_kp", omega_kp );
    g_key_file_set_double( keyfile, "Electrical", "omega_ki", omega_ki );
    g_key_file_set_double( keyfile, "Electrical", "omega_kd", omega_kd );

    g_key_file_set_double( keyfile, "Motor", "armature_resistance", Ra );
    g_key_file_set_double( keyfile, "Motor", "armature_inductance", La );
    g_key_file_set_double( keyfile, "Motor", "inertia", Jsum );
    g_key_file_set_double( keyfile, "Motor", "power", Pn );
    g_key_file_set_double( keyfile, "Motor", "speed", Nn );
    g_key_file_set_double( keyfile, "Motor", "speed_constant", kmPhi );
    g_key_file_set_integer( keyfile, "Motor", "armature_resistance_grade", combobox_motor_resistance_grade );
    g_key_file_set_integer( keyfile, "Motor", "armature_inductance_grade", combobox_motor_inductance_grade );
    g_key_file_set_integer( keyfile, "Motor", "armature_power_grade", combobox_motor_power_grade );

    g_key_file_set_integer( keyfile, "Load", "type", load_type );

    /* save the keyfile */
    mousepad_util_save_key_file (keyfile, filename);

    /* close the key file */
    g_key_file_free (keyfile);

    /* cleanup */
    g_free (filename);

    return TRUE;
}

// #############################################################################

void update_statusbar( GtkStatusbar  *statusbar, const gchar *text )
{
  /* clear any previous message, underflow is allowed */
  gtk_statusbar_pop( statusbar, 0 );

  gtk_statusbar_push( statusbar, 0, text );
}

void show_action_infobar( struct CallbackObjekt *obj, gchar *format, ... )
{
    // Write a formatted string into a GString.
    va_list arg;
    va_start( arg, format );
    GString *msg = g_string_new( NULL );
    g_string_vprintf( msg, format, arg );

    // Show the info message
    GtkWidget *label_info = g_object_get_data( G_OBJECT(obj->window), "label_info" );
    GtkWidget *infobar = g_object_get_data( G_OBJECT(obj->window), "infobar1" );
    gtk_label_set_text( GTK_LABEL (label_info), msg->str );
    gtk_widget_show(infobar);

    g_string_free( msg, TRUE );
    va_end( arg );
}

void messagebox( GtkBuilder *b, gchar *format, ... )
{
    GtkWindow *window = NULL;
    GtkWidget *dialog = NULL;

    if( b != NULL )
    {
        window = GTK_WINDOW( gtk_builder_get_object( b, applicationwindow_name ) );
    }

    // Write a formatted string into a GString.
    va_list arg;
    va_start( arg, format );
    GString *msg = g_string_new( NULL );
    g_string_vprintf( msg, format, arg );

    // Create the message dialog
    dialog = gtk_message_dialog_new( window,
        GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
        msg->str );
    gtk_window_set_position( GTK_WINDOW( dialog ), GTK_WIN_POS_CENTER );
    gtk_dialog_run( GTK_DIALOG( dialog ) );
    gtk_widget_destroy( dialog );

    g_string_free( msg, TRUE );
    va_end( arg );
}

// #############################################################################

G_MODULE_EXPORT void on_button_hide_clicked( GtkWidget *button, GtkWidget *widget )
{
    gtk_widget_hide( widget );
}

// #############################################################################

G_MODULE_EXPORT void on_button_save_clicked( GtkWidget *button, struct CallbackObjekt *obj )
{
    GtkEntry *textfeld = GTK_ENTRY( gtk_builder_get_object( obj->builder, "entry_filename" ) );
    GtkWidget *dateidialog = GTK_WIDGET( gtk_builder_get_object( obj->builder, "filechooserdialog1" ) );
    gchar *dateiname;

    // Transfer file path from entry to the file chooser.
    // Doesn't work: only existing files.
    //dateiname = (gchar *) gtk_entry_get_text( textfeld );
    //GFile *file = g_file_new_for_path( dateiname );
    //gtk_file_chooser_set_file( GTK_FILE_CHOOSER( dateidialog ), file, NULL );
    //g_object_unref( file );

    gint result = gtk_dialog_run( GTK_DIALOG( dateidialog ) );
    if( result == GTK_RESPONSE_ACCEPT )
    {
        dateiname = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( dateidialog ) );
        gtk_entry_set_text( textfeld, dateiname );
        g_free( dateiname );
    }

    gtk_widget_hide( dateidialog );
    //gtk_widget_destroy( dateidialog );
}

// #############################################################################

G_MODULE_EXPORT void on_button_calc_clicked( GtkWidget *button, struct CallbackObjekt *obj )
{
    if( obj->dc_motor_run == FALSE )
    {
        obj->dc_motor_run = TRUE;

        obj->loop_data = g_malloc( sizeof(struct DC_MOTOR_LOOP_DATA) );
        obj->loop_data->obj = obj;
        obj->loop_data->done = FALSE;
        obj->loop_data->button = button;;

        GtkEntry *textfeld = GTK_ENTRY( gtk_builder_get_object( obj->builder, "entry_filename" ) );
        const gchar *filename = gtk_entry_get_text( textfeld );

        obj->loop_data->file = g_file_new_for_path( filename );
        GError *error = NULL;
        obj->loop_data->stream = g_file_replace( obj->loop_data->file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error );
        if( error != NULL )
        {
            g_warning( "Unable to save file: %s\n", error->message );
            messagebox( obj->builder, "DC motor calculation error: %s\n", error->message );
            g_error_free( error );
            return;
        }

        //loop_data->event_source_calc = g_idle_add( (GSourceFunc) dc_motor_loop, NULL );
        obj->pthread = g_thread_new( "dc_motor_loop", (GThreadFunc) dc_motor_loop, obj );

        gtk_button_set_label( GTK_BUTTON( button ), "Stop" );

        //execl( "./DC_motor.plot", "", NULL );
    }
    else
    {
        obj->dc_motor_run = FALSE;
    }
}

// #############################################################################

G_MODULE_EXPORT void on_button_new_item_clicked( GtkWidget *button, struct CallbackObjekt *obj )
{
    GtkBuilder *builder_lokal = obj->builder;

    GtkTreeView *view = GTK_TREE_VIEW( gtk_builder_get_object( builder_lokal, "treeview_rheostat" ) );
    GtkTreeModel *model = gtk_tree_view_get_model( view );
    GtkTreeSelection *selection = gtk_tree_view_get_selection( view );

    gint selected_rows = gtk_tree_selection_count_selected_rows( selection );
    if( selected_rows == 1 )
    {
        // Get selected row
        GList *rows = gtk_tree_selection_get_selected_rows( selection, &model );

        GtkTreePath *path = (GtkTreePath *) rows[0].data;
        gint *indices = gtk_tree_path_get_indices( path );
        //gint depth = gtk_tree_path_get_depth( path );

        GtkTreeIter iter;
        gtk_list_store_insert_with_values( GTK_LIST_STORE( model ), &iter, indices[0] + 1,
            0, 0.0,
            1, 0.0,
            -1
        );

        gtk_tree_path_free( path );
    }
    else if( selected_rows == 0 )
    {
        GtkTreeIter iter;
        gtk_list_store_insert_with_values( GTK_LIST_STORE( model ), &iter, -1,
            0, 0.0,
            1, 0.0,
            -1
        );
    }
}

G_MODULE_EXPORT void on_button_del_item_clicked( GtkWidget *button, struct CallbackObjekt *obj )
{
    GtkBuilder *builder_lokal = obj->builder;

    GtkTreeView *view = GTK_TREE_VIEW( gtk_builder_get_object( builder_lokal, "treeview_rheostat" ) );
    GtkTreeModel *model = gtk_tree_view_get_model( view );
    GtkTreeSelection *selection = gtk_tree_view_get_selection( view );

    // If nothing is selected, return.
    gint selected_rows = gtk_tree_selection_count_selected_rows( selection );
    if( !selected_rows )
    {
        return;
    }

    // Get selected rows
    GList *rows = gtk_tree_selection_get_selected_rows( selection, &model );
    GList *references = NULL;

    // If we are planning on modifying the model after calling
    // gtk_tree_selection_get_selected_rows function, we have to convert the
    // returned list into a list of GtkTreeRowReferences.
    for( GList *tmp = rows; tmp; tmp = g_list_next( tmp ) )
    {
        GtkTreePath *path = (GtkTreePath *) tmp->data;
        references = g_list_append( references,
            gtk_tree_row_reference_new( model, path )
        );
    }

    // Get new path for each selected row and delete items.
    for( GList *tmp = references; tmp; tmp = g_list_next( tmp ) )
    {
        GtkTreeIter iter;
        GtkTreeRowReference *reference = (GtkTreeRowReference *) tmp->data;
        GtkTreePath *path = gtk_tree_row_reference_get_path( reference );
        gtk_tree_model_get_iter( model, &iter, path );
        gtk_list_store_remove( GTK_LIST_STORE( model ), &iter );
    }

    /* Free our paths */
    g_list_foreach( rows, (GFunc) gtk_tree_path_free, NULL );
    g_list_free( rows );
    g_list_foreach( references, (GFunc) gtk_tree_row_reference_free, NULL );
}

G_MODULE_EXPORT void on_button_move_clicked( GtkWidget *button, struct CallbackObjekt *obj )
{
    GtkBuilder *builder_lokal = obj->builder;

    /* TRUE means "move up", FALSE "move down" */
    GtkButton *button_up = GTK_BUTTON( gtk_builder_get_object( builder_lokal, "button_move_up" ) );
    //GtkButton *button_down = GTK_BUTTON( gtk_builder_get_object( builder_lokal, "button_move_down" ) );
    gboolean direction = ( GTK_BUTTON( button ) == button_up ? TRUE : FALSE );

    GtkTreeView *view = GTK_TREE_VIEW( gtk_builder_get_object( builder_lokal, "treeview_rheostat" ) );
    GtkTreeModel *model = gtk_tree_view_get_model( view );
    GtkTreeSelection *selection = gtk_tree_view_get_selection( view );

    // If nothing is selected, return.
    gint selected_rows = gtk_tree_selection_count_selected_rows( selection );
    if( !selected_rows )
    {
        return;
    }

    // Get selected rows
    GList *rows = gtk_tree_selection_get_selected_rows( selection, &model );

    /* Get new path for each selected row and swap items. */
    for( GList *tmp = rows; tmp; tmp = g_list_next( tmp ) )
    {
        GtkTreeIter  iter1, iter2;  /* Iters for swapping items. */

        /* Copy path */
        GtkTreePath *path1 = (GtkTreePath *) tmp->data;
        GtkTreePath *path2 = gtk_tree_path_copy( path1 );

        /* Move path2 in right direction */
        if( direction )
            gtk_tree_path_prev( path2 );
        else
            gtk_tree_path_next( path2 );

        /* Compare paths and skip one iteration if the paths are equal, which means we're
         * trying to move first path up. */
        if( ! gtk_tree_path_compare( path1, path2 ) )
        {
            gtk_tree_path_free( path2 );
            continue;
        }

        /* Now finally obtain iters and swap items. If the second iter is invalid, we're
         * trying to move the last item down. */
        gtk_tree_model_get_iter( model, &iter1, path1 );
        if( ! gtk_tree_model_get_iter( model, &iter2, path2 ) )
        {
            gtk_tree_path_free( path2 );
            continue;
        }
        gtk_list_store_swap( GTK_LIST_STORE( model ), &iter1, &iter2 );
        gtk_tree_path_free( path2 );
    }

    /* Free our paths */
    g_list_foreach( rows, (GFunc) gtk_tree_path_free, NULL );
    g_list_free( rows );
}

// #############################################################################

G_MODULE_EXPORT void on_treeview_rheostat_edited( GtkCellRendererText *renderer, gchar *pfad, gchar *neuer_text, struct CallbackObjekt *obj )
{

    gdouble angular_speed;
    gdouble armature_current;

    GtkBuilder *builder_lokal = obj->builder;
    GtkTreeModel *model = GTK_TREE_MODEL( gtk_builder_get_object( builder_lokal, "liststore_rheostat" ) );
    GtkCellRendererText *renderer6 = GTK_CELL_RENDERER_TEXT( gtk_builder_get_object( builder_lokal, "cellrenderertext6" ) );
    GtkCellRendererText *renderer7 = GTK_CELL_RENDERER_TEXT( gtk_builder_get_object( builder_lokal, "cellrenderertext7" ) );

    GtkTreeIter iter;
    gtk_tree_model_get_iter_from_string( model, &iter, pfad );

    /* Jetzt holen wir uns die Daten aus geänderter Zeile */
    gtk_tree_model_get( model, &iter,
      0, &angular_speed,
      1, &armature_current,
      -1 );

    if( renderer == renderer6 )
    {
        angular_speed = g_strtod( neuer_text, NULL );
    }
    else if( renderer == renderer7 )
    {
        armature_current = g_strtod( neuer_text, NULL );

    }

    /* Daten setzen */
    gtk_list_store_set( GTK_LIST_STORE( model ), &iter,
      0, angular_speed,
      1, armature_current,
      -1 );
}

// #############################################################################

G_MODULE_EXPORT void on_window_destroy( GtkWidget *widget, gpointer user_data )
{
    settings_store( );
    gtk_main_quit( );
}

// #############################################################################

void init( struct CallbackObjekt *obj )
{
    GtkWidget *message = (GtkWidget *) gtk_builder_get_object( obj->builder, "label_info" );
    GtkWidget *infobar = (GtkWidget *) gtk_builder_get_object( obj->builder, "infobar1" );
    g_object_set_data( G_OBJECT(obj->window), "label_info", message );
    g_object_set_data( G_OBJECT(obj->window), "infobar1", infobar );

    const gchar *default_filename = "DC_motor.dat";

    GtkWidget *dateidialog = GTK_WIDGET( gtk_builder_get_object( obj->builder, "filechooserdialog1" ) );
    gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER( dateidialog ), "./" );
    gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER( dateidialog ), default_filename );

    gchar *current_dir = g_get_current_dir( );
    GString *filename = g_string_new( NULL );
    //g_string_append( filename, current_dir );
    g_string_append( filename, "./" );
    g_string_append( filename, default_filename );
    GtkEntry *textfeld = GTK_ENTRY( gtk_builder_get_object( obj->builder, "entry_filename" ) );
    gtk_entry_set_text( textfeld, filename->str );
    g_string_free( filename, TRUE );
    g_free( current_dir );

    settings_load( );
    dc_motor_init( obj );
    plot_init( obj );
}

// #############################################################################

int main( int argc, char *argv[] )
{
    struct CallbackObjekt *callobj;
    GError *error = NULL;

    // Initialize GTK+
    g_log_set_handler( "Gtk", G_LOG_LEVEL_WARNING,( GLogFunc ) gtk_false, NULL );
    gtk_init( &argc, &argv );
    g_log_set_handler( "Gtk", G_LOG_LEVEL_WARNING, g_log_default_handler, NULL );

    callobj = g_malloc0( sizeof(struct CallbackObjekt ) );

    // Parse a file containing a GtkBuilder UI definition and merges it with
    // the contents of builder
    callobj->builder = gtk_builder_new( );
    gtk_builder_add_from_file( callobj->builder, workspace_filename, &error );
    //g_assert(error == NULL);
    if( error != NULL )
    {
        g_warning( "Unable to parse GUI XML: %s\n", error->message );
        g_error_free( error );
        return EXIT_FAILURE;
    }

    // Get main window object and match the signal handler names given in the
    // interface description with symbols in the application and connects the
    // signals
    callobj->window = GTK_WIDGET( gtk_builder_get_object( callobj->builder, applicationwindow_name ) );
    g_assert( callobj->window != NULL );
    gtk_builder_connect_signals( callobj->builder, callobj );

    init( callobj );

    // Enter the main loop
    gtk_widget_show_all( callobj->window );
    gtk_main( );

    // We are no longer going to use the GtkBuilder object. We used it to
    // construct our widgets and then we obtained pointers to the widgets we
    // needed to reference. So now we can free all the memory it used up with
    // XML stuff.
    g_object_unref( G_OBJECT( callobj->builder ) );

    /* Aufräumen */

    plot_deinit( callobj );

    g_free( callobj );

    return EXIT_SUCCESS;
}

// #############################################################################

void gtk_label_printf( struct CallbackObjekt *obj, const gchar *name, gchar *format, ... )
{
    // Write a formatted string into a GString.
    va_list arg;
    va_start( arg, format );
    GString *msg = g_string_new( NULL );
    g_string_vprintf( msg, format, arg );

    // Put formatted string on the label
    GtkLabel *label = GTK_LABEL( gtk_builder_get_object( obj->builder, name ) );
    if( label != NULL )
    {
        gtk_label_set_text( label, msg->str );
    }

    g_string_free( msg, TRUE );
    va_end( arg );
}

// #############################################################################

// This was suddenly added in glib-2.4.0, woo
gssize g_output_stream_printff( GFileOutputStream *stream, gchar *format, ... )
{
    // Write a formatted string into a GString.
    va_list arg;
    va_start( arg, format );
    GString *msg = g_string_new( NULL );
    g_string_vprintf( msg, format, arg );

    // Create the message dialog
    GError *error = NULL;
    gssize written = g_output_stream_write( G_OUTPUT_STREAM(stream), msg->str, msg->len, NULL, &error );
    if( error != NULL )
    {
        g_warning( "Unable to write file: %s\n", error->message );
        messagebox( NULL, "DC motor calculation error: %s\n", error->message );
        g_error_free( error );
        return 0;
    }

    g_string_free( msg, TRUE );
    va_end( arg );

    return written;
}

// #############################################################################
