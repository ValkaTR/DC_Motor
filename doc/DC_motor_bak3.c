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

extern gboolean dc_motor_run;

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

extern struct DC_MOTOR_LOOP_DATA *loop_data;
extern struct DC_MOTOR_TIMEOUT_DATA *timeout_data;

/** The function is called repeatedly until it returns FALSE, at which point
 * the timeout is automatically destroyed and the function will not be called
 * again. The first call to the function will be at the end of the first
 * interval.
 */
gboolean dc_motor_source_func( gpointer user_data )
{
    GtkProgressBar *progress = GTK_PROGRESS_BAR( gtk_builder_get_object( timeout_data->obj->builder, "progressbar1" ) );

    gtk_progress_bar_set_fraction( progress, *(timeout_data->t) / timeout_data->t1 );

    gtk_label_printf( timeout_data->obj, "label_starting_current", "%g A", *(timeout_data->i_max) );

    return ( *(timeout_data->done) == TRUE ? FALSE : TRUE );
}

/** The routines solve the general n-dimensional first-order system,
 *
 *      dy_i(t) / dt = f_i(t, y_1(t), ..., y_n(t))
 *
 * for i = 1, …, n. The stepping functions rely on the vector of
 * derivatives f_i
 *
 */
gboolean dc_motor_loop( gpointer user_data )
{
    //
    // Calculation loop
    //

    //double time_print = 0.0;
    if( loop_data->t < loop_data->t1 )
    {
        //int status = gsl_odeiv2_evolve_apply_fixed_step(
        //    loop_data->ode_evolve, loop_data->ode_control, loop_data->ode_step, &loop_data->ode_sys,
        //    &loop_data->t,     /* System time (input, output) */
        //    loop_data->h,      /* Step-size (input, output) */
        //    loop_data->y       /* System position (input, output) */
        //);
        //loop_data->t += loop_data->h;

        // Adaptive evolve
        int status = gsl_odeiv2_evolve_apply(
            loop_data->ode_evolve, loop_data->ode_control, loop_data->ode_step, &(loop_data->ode_sys),
            &loop_data->t,     /* System time (input, output) */
            loop_data->t1,     /* Maximum time (input) */
            &loop_data->h,     /* Step-size (input, output) */
            loop_data->y       /* System position (input, output) */
        );

        if( status != GSL_SUCCESS )
            return FALSE;

        //if( ( time_print + 0.001 ) < t )
        {
            g_ascii_dtostr( loop_data->tmp_buf, 50, loop_data->t );
            g_string_append( loop_data->out, loop_data->tmp_buf );
            g_string_append( loop_data->out, "\t" );

            g_ascii_dtostr( loop_data->tmp_buf, 50, 520 );
            g_string_append( loop_data->out, loop_data->tmp_buf );
            g_string_append( loop_data->out, "\t" );

            g_ascii_dtostr( loop_data->tmp_buf, 50, loop_data->y[0] );
            g_string_append( loop_data->out, loop_data->tmp_buf );
            g_string_append( loop_data->out, "\t" );

            g_ascii_dtostr( loop_data->tmp_buf, 50, loop_data->y[1] / ( 2*M_PI / 60 ) );
            g_string_append( loop_data->out, loop_data->tmp_buf );
            g_string_append( loop_data->out, "\n" );

            g_output_stream_printf( loop_data->stream, "%s", loop_data->out->str );
            //printf( "%s", out->str );
            g_string_set_size( loop_data->out, 0 );

			//time_print = t;
        }

        if( loop_data->i_max < loop_data->y[0] ) loop_data->i_max = loop_data->y[0];

        usleep( 20*1000 );
    }
    else
    {
        //
        // Calculations finished
        //

        GtkBuilder *builder = loop_data->obj->builder;

        dc_motor_stop( NULL );

        messagebox( builder, "DC motor calculated and data is save into file!" );

        return FALSE;
    }

    return TRUE;
}

gboolean dc_motor_stop( gpointer user_data )
{
    loop_data->done = TRUE;

    g_source_remove( loop_data->event_source_id );
    g_source_remove( loop_data->event_source_calc );

    g_free( loop_data->tmp_buf );
    g_string_free( loop_data->out, TRUE );

    gsl_odeiv2_evolve_free( loop_data->ode_evolve );
    gsl_odeiv2_control_free( loop_data->ode_control );
    gsl_odeiv2_step_free( loop_data->ode_step );

    GError *error = NULL;
    g_output_stream_flush( G_OUTPUT_STREAM( loop_data->stream ), NULL, &error );
    if( error != NULL )
    {
        g_warning( "Unable to flush file: %s\n", error->message );
        messagebox( loop_data->obj->builder, "DC motor calculation error: %s\n", error->message );
        g_error_free( error );
        return FALSE;
    }

    g_object_unref( loop_data->stream );
    g_object_unref( loop_data->file );

    GtkButton *button = GTK_BUTTON( loop_data->button );

    g_free( loop_data );
    g_free( timeout_data );

    loop_data = NULL;
    timeout_data = NULL;

    gtk_button_set_label( button, "Calculate" );

    return FALSE;
}

// #############################################################################
