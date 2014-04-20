// #############################################################################

#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <math.h>

#include <gsl/gsl_math.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_odeiv2.h>

// #############################################################################

#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED

// #############################################################################

#define GROUP_NAME "Configuration"
#define DC_MOTOR_RC_RELPATH     ("DC_motor" G_DIR_SEPARATOR_S "dc_motor_rc")

/* Ein Objekt, das allen Callbacks übergeben wird */
struct CallbackObjekt
{
    GtkBuilder *builder;
    GtkWidget *window;
    GtkWidget *drawingarea;

    GThread *pthread;
    gboolean dc_motor_run;
    struct DC_MOTOR_LOOP_DATA *loop_data;
    struct DC_MOTOR_TIMEOUT_DATA *timeout_data;
};

struct DC_MOTOR_LOOP_DATA
{
    GtkWidget *button;

    struct CallbackObjekt *obj;
    GFile *file;
    GFileOutputStream *stream;

    gsl_odeiv2_step *ode_step;
    gsl_odeiv2_control *ode_control;
    gsl_odeiv2_evolve *ode_evolve;

    gsl_odeiv2_system ode_sys;

    gsl_odeiv2_step_type *step_type;
    double eps_abs; /**< Absolute error */
    double eps_rel; /**< Relative error */
    double a_y; /**< Scaling factor for the system state y(t) */
    double a_dydt; /**< Scaling factor for the system state y'(t) */

    double t; /**< Start time */
    double t1; /**< End time */
    double h; /**< Initial timestep */
    double y[2]; /** Initial conditions of the system */
    double i_max; /**< Variable to track peak current */

    GString *out;
    gchar *tmp_buf;
    GString *buffer;

    guint event_source_timeout;

    gboolean done;
};

enum RHEOSTAT_MODE
{
    RHEOSTAT_MODE_NONE = 0,
    RHEOSTAT_MODE_SPEED,
    RHEOSTAT_MODE_CURRENT
};

enum REGULATOR_STATE
{
    REGULATOR_STATE_NONE = 0,
    REGULATOR_STATE_SPEED,
    REGULATOR_STATE_CURRENT_LIMIT
};

enum REGULATOR_MODE
{
    REGULATOR_MODE_NONE = 0,
    REGULATOR_MODE_SPEED
};

// #############################################################################

gboolean settings_init( struct CallbackObjekt *obj );
gboolean settings_update( struct CallbackObjekt *obj );
gboolean settings_load( );
gboolean settings_store( );

void show_action_infobar( struct CallbackObjekt *obj, gchar *format, ... ) G_GNUC_PRINTF (2, 3);
void messagebox( GtkBuilder *b, gchar *format, ... ) G_GNUC_PRINTF (2, 3);

gboolean dc_motor_init( struct CallbackObjekt *obj );
int dc_motor_model( double t, const double y[], double f[], void *params );
gboolean dc_motor_source_func( gpointer user_data );
gpointer dc_motor_loop( gpointer userdata );
gboolean dc_motor_stop( gpointer user_data );

void gtk_label_printf( struct CallbackObjekt *obj, const gchar *name, gchar *format, ... );
//gssize g_output_stream_printf( GFileOutputStream *stream, gchar *format, ... );

// #############################################################################

#endif // MAIN_H_INCLUDED

// #############################################################################
