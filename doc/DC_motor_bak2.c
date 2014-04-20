// #############################################################################

#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <math.h>

#include <gsl/gsl_math.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_odeiv2.h>

#include "main.h"

// #############################################################################

const gchar *motor_type = "Siemens 1GG6166-0JC40-7MV5";

// R( θ₀ ) = 120 °C
// R( θ ) = R( θ₀ ) · ( 1 + α · Δθ )
// Δθ = θ₂ - θ₁
// α_copper = 0.003862 K¯¹
#define copper_alpha    0.003862
double Ra = ( 1 + 0.8 ) * 0.932 * ( 1 + copper_alpha * ( 20 - 120 ) );

double La = 11.5 / 1000; // ?!
double Jsum = 2.44 * ( 1 + 2 ); // sumaarne inertsimoment
double Pn = 36 * 1000; // W
double Nn = 710;  // p/min
double Un = 520;
const double kmPhi = 5.7619047619;

double Dt = 0.00001;

// #############################################################################

gboolean dc_motor_init( struct CallbackObjekt *obj )
{

    gtk_label_printf( obj, "label_motor_type", "%s", motor_type );
    gtk_label_printf( obj, "label_mechanical_power", "%g kW", Pn / 1000 );
    gtk_label_printf( obj, "label_armature_resistance", "%g Ω", Ra );

    return TRUE;
}

// #############################################################################

/** \brief Mechanical load calculation
 *
 * \param omega Rotor speed, rad¯¹
 * \return Load torque, N·m
 *
 */
double mechanical_load( double omega )
{
	double omega_n, Tn, Tkoormus;

	// Mootori nimikiirus, rad/s
	omega_n = Nn * ( 2*M_PI / 60 );

	// nimimoment
	Tn = Pn / omega_n;

	// Koormuse variant a) - konstantne
	Tkoormus = 0.7 * Tn;

	// Koormuse variant b) - ventilaatorkoormus
	Tkoormus = 0.7 * Tn * pow( omega / omega_n, 2 );

    // Koormuse variant c) - tühijaoks
    //Tkoormus = 0.0;

	return Tkoormus;
}

// #############################################################################

/** \brief Mechanical model calculation
 *
 * \param Te Torque, N·m
 * \param omega Rotor speed, rad¯¹
 * \return dω/dt of the mechanical model
 *
 */
double mechanical_model( double Te, double omega )
{
    // Tm = kΦ·i
    // Ts = f(ω)
    double Ts = mechanical_load( omega );
    double Teff = Te - Ts;

    // J·(dω / dt) = Tm - Ts
    // ω = dφ / dt
    // dω / dt = (Tm - Ts) / J;
    double f = Teff / Jsum;

    return f;
}

// #############################################################################

/** \brief Electrical model calculation
 *
 * \param Ua Armature voltage, V
 * \param ia Armature current, A
 * \param omega Rotor speed, rad¯¹
 * \return di/dt of the electrical model
 *
 */
double electrical_model( double Ua, double ia, double omega )
{
    // E = kΦ·ω
    double E = kmPhi * omega;

    // U = i·R + L (di / dt) + E
    // di / dt = (U - i·R - E) / L
    double f = (Ua - ia * Ra - E) / La;

    return f;
}

// #############################################################################

/** \brief This function should store the vector elements f_i(t, y, params) in
 * the array dydt, for arguments (t, y) and parameters params.
 *
 * \param[in] t System time
 * \param[in] y System position
 * \param[out] f System output
 * \param[in] params User-defined parameters
 * \return The function should return GSL_SUCCESS if the calculation was
 * completed successfully. Any other return value indicates an error.
 *
 */
int dc_motor_model( double t, const double y[], double f[], void *params )
{
    //double mu = *(double *) params;
    //f[0] = y[1]; //y[1];
    //f[1] = 1500 - y[1]; //-y[0] - mu*y[1]*(y[0]*y[0] - 1);

    double i = y[0];
    double omega = y[1];

    f[0] = electrical_model( 520, i, omega );
    f[1] = mechanical_model( i * kmPhi, omega );

    return GSL_SUCCESS;
}

// #############################################################################

/** The function is called repeatedly until it returns FALSE, at which point
 * the timeout is automatically destroyed and the function will not be called
 * again. The first call to the function will be at the end of the first
 * interval.
 */
gboolean dc_motor_source_func( gpointer user_data )
{
    struct DC_MOTOR_TIMEOUT_DATA
    {
        struct CallbackObjekt *obj;
        GFile *file;
        GFileOutputStream *stream;
        double *t;
        double t1;
        double *i_max;
        gboolean *done;
    }
    *timeout_data = (struct DC_MOTOR_TIMEOUT_DATA *) user_data;

    GtkProgressBar *progress = GTK_PROGRESS_BAR( gtk_builder_get_object( timeout_data->obj->builder, "progressbar1" ) );

    gtk_progress_bar_set_fraction( progress, *(timeout_data->t) / timeout_data->t1 );

    gtk_label_printf( timeout_data->obj, "label_starting_current", "%g A", *(timeout_data->i_max) );

    return ( *(timeout_data->done) ? FALSE : TRUE );
}


/** The routines solve the general n-dimensional first-order system,
 *
 *      dy_i(t) / dt = f_i(t, y_1(t), ..., y_n(t))
 *
 * for i = 1, …, n. The stepping functions rely on the vector of
 * derivatives f_i
 *
 */
//gboolean dc_motor_run( struct CallbackObjekt *obj, GFileOutputStream *stream )
gboolean dc_motor_run( gpointer user_data )
{
    struct DC_MOTOR_LOOP_DATA
    {
        struct CallbackObjekt *obj;
        GFile *file;
        GFileOutputStream *stream;
    }
    *loop_data = (struct DC_MOTOR_LOOP_DATA *) user_data;

    const gsl_odeiv2_step_type *step_type = gsl_odeiv2_step_rk8pd;
    const double eps_abs = 1e-9; /**< Absolute error */
    const double eps_rel = 0.0; /**< Relative error */
    const double a_y = 1.0; /**< Scaling factor for the system state y(t) */
    const double a_dydt = 0.0; /**< Scaling factor for the system state y'(t) */

    double t = 0.0; /**< Start time */
    double t1 = 2.0; /**< End time */
    double h = 1e-6; /**< Initial timestep */
    double y[2] = { 0.0, 0.0 }; /** Initial conditions of the system */
    double i_max = 0.0; /**< Variable to track peak current */

    gboolean done = FALSE;

    // Buffer for serialization with decimal point '.'
    GString *out = g_string_new( NULL );
    gchar *tmp_buf = g_malloc( 50 );

    // File header
    g_output_stream_printf( loop_data->stream, "# t \t U \t i \t omega\n" );
    //printf( "# t \t U \t i \t omega\n" );

    struct DC_MOTOR_TIMEOUT_DATA
    {
        struct CallbackObjekt *obj;
        GFile *file;
        GFileOutputStream *stream;
        double *t;
        double t1;
        double *i_max;
        gboolean *done;
    }
    timeout_data =
    {
        .obj = loop_data->obj,
        .file = loop_data->file,
        .stream = loop_data->stream,
        .t = &t,
        .t1 = t1,
        .i_max = &i_max,
        .done = &done
    };
    //gint event_source_id = g_timeout_add( 20 /* ms */, (GSourceFunc) dc_motor_source_func, &timeout_data );

    //
    // Initialize ODE solver
    //

    gsl_odeiv2_step *ode_step = gsl_odeiv2_step_alloc( step_type, 2 );
    gsl_odeiv2_control *ode_control = gsl_odeiv2_control_standard_new( eps_abs, eps_rel, a_y, a_dydt );
    gsl_odeiv2_evolve *ode_evolve = gsl_odeiv2_evolve_alloc( 2 );

    double mu = 10;
    gsl_odeiv2_system ode_sys = { dc_motor_model, NULL, 2, &mu };

    //
    // Calculation loop
    //

    //double time_print = 0.0;
    while( t < t1 )
    {
        //int status = gsl_odeiv2_evolve_apply_fixed_step(
        //    ode_evolve, ode_control, ode_step, &ode_sys,
        //    &t,     /* System time (input, output) */
        //    h,      /* Step-size (input, output) */
        //    y       /* System position (input, output) */
        //);
        //t += h;

        // Adaptive evolve
        int status = gsl_odeiv2_evolve_apply(
            ode_evolve, ode_control, ode_step, &ode_sys,
            &t,     /* System time (input, output) */
            t1,     /* Maximum time (input) */
            &h,     /* Step-size (input, output) */
            y       /* System position (input, output) */
        );

        if( status != GSL_SUCCESS )
            break;

        //if( ( time_print + 0.001 ) < t )
        {
            g_ascii_dtostr( tmp_buf, 50, t );
            g_string_append( out, tmp_buf );
            g_string_append( out, "\t" );

            g_ascii_dtostr( tmp_buf, 50, 520 );
            g_string_append( out, tmp_buf );
            g_string_append( out, "\t" );

            g_ascii_dtostr( tmp_buf, 50, y[0] );
            g_string_append( out, tmp_buf );
            g_string_append( out, "\t" );

            g_ascii_dtostr( tmp_buf, 50, y[1] / ( 2*M_PI / 60 ) );
            g_string_append( out, tmp_buf );
            g_string_append( out, "\n" );

            g_output_stream_printf( loop_data->stream, "%s", out->str );
            //printf( "%s", out->str );
            g_string_set_size( out, 0 );

			//time_print = t;
        }

        if( i_max < y[0] ) i_max = y[0];

        usleep( 20*1000 );
    }
return FALSE;
    //
    // Calculations finished
    //

    done = TRUE;
    //usleep( 30*1000 );
    //g_source_remove( event_source_id );
    //g_main_context_invoke( NULL, (GSourceFunc) dc_motor_source_func, &timeout_data );

    g_free( tmp_buf );
    g_string_free( out, TRUE );

    gsl_odeiv2_evolve_free( ode_evolve );
    gsl_odeiv2_control_free( ode_control );
    gsl_odeiv2_step_free( ode_step );

    //return TRUE;
    return FALSE;
}

// #############################################################################
