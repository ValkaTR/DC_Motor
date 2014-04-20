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

#include "main.h"

// #############################################################################

const gchar *workspace_filename = "workspace.xml";
const gchar *applicationwindow_name = "applicationwindow1";

// #############################################################################

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

struct DC_MOTOR_LOOP_DATA *loop_data = NULL;
struct DC_MOTOR_TIMEOUT_DATA *timeout_data = NULL;

G_MODULE_EXPORT void on_button_calc_clicked( GtkWidget *button, struct CallbackObjekt *obj )
{
    if( loop_data == NULL || ( loop_data != NULL && loop_data->done == TRUE )  )
    {
        loop_data = g_malloc( sizeof(struct DC_MOTOR_LOOP_DATA) );
        loop_data->obj = obj;
        loop_data->done = FALSE;
        loop_data->button = button;;

        GtkEntry *textfeld = GTK_ENTRY( gtk_builder_get_object( obj->builder, "entry_filename" ) );
        const gchar *filename = gtk_entry_get_text( textfeld );

        loop_data->file = g_file_new_for_path( filename );
        GError *error = NULL;
        loop_data->stream = g_file_replace( loop_data->file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error );
        if( error != NULL )
        {
            g_warning( "Unable to save file: %s\n", error->message );
            messagebox( obj->builder, "DC motor calculation error: %s\n", error->message );
            g_error_free( error );
            return;
        }

        loop_data->step_type = (gsl_odeiv2_step_type *) gsl_odeiv2_step_rk8pd;
        loop_data->eps_abs = 1e-9; /**< Absolute error */
        loop_data->eps_rel = 0.0; /**< Relative error */
        loop_data->a_y = 1.0; /**< Scaling factor for the system state y(t) */
        loop_data->a_dydt = 0.0; /**< Scaling factor for the system state y'(t) */

        loop_data->t = 0.0; /**< Start time */
        loop_data->t1 = 2.0; /**< End time */
        loop_data->h = 1e-6; /**< Initial timestep */
        loop_data->y[0] = 0.0; /** Initial conditions of the system */
        loop_data->y[1] = 0.0; /** Initial conditions of the system */
        loop_data->i_max = 0.0; /**< Variable to track peak current */

        // Buffer for serialization with decimal point '.'
        loop_data->out = g_string_new( NULL );
        loop_data->tmp_buf = g_malloc( 50 );

        // File header
        g_output_stream_printf( loop_data->stream, "# t \t U \t i \t omega\n" );
        //printf( "# t \t U \t i \t omega\n" );

        timeout_data = g_malloc( sizeof(struct DC_MOTOR_TIMEOUT_DATA) );
        timeout_data->obj = loop_data->obj;
        timeout_data->file = loop_data->file;
        timeout_data->stream = loop_data->stream;
        timeout_data->t = &(loop_data->t);
        timeout_data->t1 = loop_data->t1;
        timeout_data->i_max = &(loop_data->i_max);
        timeout_data->done = &(loop_data->done);
        timeout_data->event_source_id = g_timeout_add( 20 /* ms */, (GSourceFunc) dc_motor_source_func, NULL );
        loop_data->timeout_data = timeout_data;
        loop_data->event_source_id = timeout_data->event_source_id;

        //
        // Initialize ODE solver
        //

        loop_data->ode_step = gsl_odeiv2_step_alloc( loop_data->step_type, 2 );
        loop_data->ode_control = gsl_odeiv2_control_standard_new( loop_data->eps_abs, loop_data->eps_rel, loop_data->a_y, loop_data->a_dydt );
        loop_data->ode_evolve = gsl_odeiv2_evolve_alloc( 2 );

        loop_data->ode_sys.function = dc_motor_model;
        loop_data->ode_sys.jacobian = NULL;
        loop_data->ode_sys.dimension = 2;
        loop_data->ode_sys.params = NULL;

        loop_data->event_source_calc = g_idle_add( (GSourceFunc) dc_motor_loop, NULL );

        gtk_button_set_label( GTK_BUTTON( button ), "Stop" );

        //execl( "./DC_motor.plot", "", NULL );
    }
    else
    {
        GtkBuilder *builder = loop_data->obj->builder;

        dc_motor_stop( loop_data );

        messagebox( builder, "DC motor calculation aborted!" );
    }
}

// #############################################################################

G_MODULE_EXPORT void on_window_destroy( GtkWidget *widget, gpointer user_data )
{
    gtk_main_quit( );
}

// #############################################################################

void init( struct CallbackObjekt *obj )
{
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

    dc_motor_init( obj );
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

    callobj = g_malloc( sizeof(struct CallbackObjekt ) );

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
    GtkWidget *window = GTK_WIDGET( gtk_builder_get_object( callobj->builder, applicationwindow_name ) );
    g_assert( window != NULL );
    gtk_builder_connect_signals( callobj->builder, callobj );

    init( callobj );

    // Enter the main loop
    gtk_widget_show_all( window );
    gtk_main( );

    // We are no longer going to use the GtkBuilder object. We used it to
    // construct our widgets and then we obtained pointers to the widgets we
    // needed to reference. So now we can free all the memory it used up with
    // XML stuff.
    g_object_unref( G_OBJECT( callobj->builder ) );

    /* Aufräumen */
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

gssize g_output_stream_printf( GFileOutputStream *stream, gchar *format, ... )
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
