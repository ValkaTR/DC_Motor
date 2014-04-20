// #############################################################################

#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <math.h>

#include <gsl/gsl_math.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_odeiv2.h>

#include <plplot.h>

#include "main.h"
#include "plot.h"
#include "DC_motor.h"

// #############################################################################

extern GArray *x_array;
extern GArray *y1_array;
extern GArray *y2_array;
extern GArray *y3_array;

// #############################################################################

const gchar *motor_type = "Siemens 1GG6166-0JC40-7MV5";

// R( θ₀ ) = 120 °C
// R( θ ) = R( θ₀ ) · ( 1 + α · Δθ )
// Δθ = θ₂ - θ₁
// α_copper = 0.003862 K¯¹

enum LOAD_TYPE load_type = 0;

double temperature = 20;
double electrical_resistivity = 0.003862;
double Ra = ( 1 + 0.8 ) * 0.932 * ( 1 + 0.003862 * ( /*20*/75 - 120 ) );
double La = 11.5 / 1000; // ?!
double Jsum = 2.44 * ( 1 + 2 ); // sumaarne inertsimoment
double Pn = 36 * 1000; // W
double Nn = 710;  // p/min
double Un = 520;
double U = 0;
double kmPhi = 5.7619047619;

double load_torque = 0.7;

double Dt = 0.00001;

// PID
double current_kp = 2.0;
double current_ki = 0.5;
double current_kd = 0.0;
double current_error_sum = 0.0;
double current_error_sum_max = 10000;
double current_error_sum_min = -10000;
double current_error_old = 0;

double omega_kp = 2.0;
double omega_ki = 0.5;
double omega_kd = 0.0;
double omega_error_sum = 0.0;
double omega_error_sum_max = 10000;
double omega_error_sum_min = -10000;
double omega_error_old = 0;

double output_coeff = 0.0;

// #############################################################################

enum RHEOSTAT_MODE rheostat_mode = RHEOSTAT_MODE_NONE;
enum REGULATOR_STATE regulator_state = REGULATOR_STATE_NONE;
enum REGULATOR_MODE regulator_mode = REGULATOR_MODE_NONE;

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
double mechanical_load( double time, double omega )
{
	double omega_n, Tn, Tkoormus = 0.0;

    //if( omega <= 0.0 )
    //    return 0.0;

	// Mootori nimikiirus, rad/s
	omega_n = Nn * ( 2*M_PI / 60 );

	// nimimoment
	Tn = Pn / omega_n;

    switch( load_type )
    {
        case LOAD_TYPE_CONSTANT:
        {
            // Koormuse variant a) - konstantne
            Tkoormus = load_torque * Tn;
            break;
        }

        case LOAD_TYPE_FAN:
        {
            // Koormuse variant b) - ventilaatorkoormus
            Tkoormus = load_torque * Tn * pow( omega / omega_n, 2 );
            break;
        }

        case LOAD_TYPE_NONE:
        {
            // Koormuse variant c) - tühijaoks
            Tkoormus = 0.0;
            break;
        }
    }

    if( time > time_xmax / 2.0 )
    {
        Tkoormus *= 2.0;
    }

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
double mechanical_model( double time, double Te, double omega )
{
    // Tm = kΦ·i
    // Ts = f(ω)
    double Ts = mechanical_load( time, omega );
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
double electrical_model( double t, double Ua, double ia, double omega )
{
    // E = kΦ·ω
    double E = kmPhi * omega;

    // U = i·R + L (di / dt) + E
    // di / dt = (U - i·R - E) / L
    double f = (Ua - ia * Ra - E) / La;

    return f;
}

// #############################################################################

double Rh = 0.0;
int rheostat_stage = 0;
int rheostat_step = 0;

struct MULTISAGE_RHEOSTAT
{
    double omega;
    double resistance;
}
rheostat[] =
{
    { 0.000000000000000, 2.043825093852814 },
    { 23.49794981609513, 1.265151325519047 },
    { 40.49334305164067, 0.7019589194321196 },
    { 52.78562359064249, 0.2946180031550676 },
    { 61.67627759054524, 0.0000000000000000 },
};

double Imin = 133.6605403666977;
double Imax = 84;//184.8;

const int rheostat_n = sizeof(rheostat) / sizeof(struct MULTISAGE_RHEOSTAT);

double rheostat_speed_model( double current, double omega )
{
    //
    // Switch by angular speed
    //

    for( int i = rheostat_n - 1; i > -1; i-- )
    {
        if( rheostat[i].omega <= omega )
        {
            rheostat_stage = i;
            break;
        }
    }

    Rh = rheostat[rheostat_stage].resistance;

    return Rh;
}

double rheostat_current_model( double current, double omega )
{

    //
    // Switch by current
    //

    if( rheostat_step == 0 && current > Imin )
    {
        rheostat_step = 1;
    }
    else if( rheostat_stage >= rheostat_n - 1 )
    {

    }
    else if( rheostat_step % 2 == 1 && current < Imin )
    {
        rheostat_step += 1;
        rheostat_stage += 1;
    }
    else if( rheostat_step % 2 == 0 && current > Imin )
    {
        rheostat_step += 1;
    }

    Rh = rheostat[rheostat_stage].resistance;

    return Rh;
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

    f[0] = electrical_model( t, U - Rh*i, i, omega );
    f[1] = mechanical_model( t, i * kmPhi, omega );

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
    struct CallbackObjekt *obj = (struct CallbackObjekt *) user_data;
    struct DC_MOTOR_LOOP_DATA *loop_data = obj->loop_data;

    GtkProgressBar *progress = GTK_PROGRESS_BAR( gtk_builder_get_object( obj->builder, "progressbar1" ) );

    gtk_progress_bar_set_fraction( progress, loop_data->t / loop_data->t1 );

    gtk_label_printf( obj, "label_starting_current", "%g A", loop_data->i_max );

    return ( obj->dc_motor_run == FALSE ? FALSE : TRUE );
}

/** The routines solve the general n-dimensional first-order system,
 *
 *      dy_i(t) / dt = f_i(t, y_1(t), ..., y_n(t))
 *
 * for i = 1, …, n. The stepping functions rely on the vector of
 * derivatives f_i
 *
 */
gpointer dc_motor_loop( gpointer user_data )
{
    struct CallbackObjekt *obj = (struct CallbackObjekt *) user_data;
    struct DC_MOTOR_LOOP_DATA *loop_data = obj->loop_data;

    loop_data->step_type = (gsl_odeiv2_step_type *) gsl_odeiv2_step_rk8pd;
    loop_data->eps_abs = 1e-5; /**< Absolute error */
    loop_data->eps_rel = 0.0; /**< Relative error */
    loop_data->a_y = 1.0; /**< Scaling factor for the system state y(t) */
    loop_data->a_dydt = 0.0; /**< Scaling factor for the system state y'(t) */

    loop_data->t = 0.0; /**< Start time */
    loop_data->t1 = time_xmax; /**< End time */
    loop_data->h = 1e-6; /**< Initial timestep */
    loop_data->y[0] = 0.0; /** Initial conditions of the system */
    loop_data->y[1] = 0.0; /** Initial conditions of the system */
    loop_data->i_max = 0.0; /**< Variable to track peak current */

    // Buffer for serialization with decimal point '.'
    loop_data->out = g_string_new( NULL );
    loop_data->tmp_buf = g_malloc( 100 );
    loop_data->buffer = g_string_new_len( "", 150 );

    // File header
    g_output_stream_printf( G_OUTPUT_STREAM(loop_data->stream), NULL, NULL, NULL, "# t \t U \t i \t omega\n" );
    //printf( "# t \t U \t i \t omega\n" );

    loop_data->event_source_timeout = g_timeout_add( 20 /* ms */, (GSourceFunc) dc_motor_source_func, obj );

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

    g_array_set_size( x_array, 0 );
    g_array_set_size( y1_array, 0 );
    g_array_set_size( y2_array, 0 );
    g_array_set_size( y3_array, 0 );

    omega_error_sum = 0.0;
    current_error_sum = 0.0;

    //
    // Calculation loop
    //

    double omega_setpoint = 0.5 * 2 * M_PI * Nn / 60;
    double current_setpoint = Imax;
    regulator_state = REGULATOR_STATE_NONE;

    //double time_print = 0.0;
    while( (loop_data->t < loop_data->t1) && obj->dc_motor_run == TRUE )
    {
        //int status = gsl_odeiv2_evolve_apply_fixed_step(
        //    loop_data->ode_evolve, loop_data->ode_control, loop_data->ode_step, &loop_data->ode_sys,
        //    &loop_data->t,     /* System time (input, output) */
        //    loop_data->h,      /* Step-size (input, output) */
        //    loop_data->y       /* System position (input, output) */
        //);
        //loop_data->t += loop_data->h;

        double *current = &loop_data->y[0];
        double *omega = &loop_data->y[1];

        //if( loop_data->t > time_xmax / 2.0 )
        //    omega_setpoint = 0.8 * 2 * M_PI * Nn / 60;

        //
        //
        //

        // Kiiruse vea arvutus
        double omega_error = omega_setpoint - *omega;
        double current_error = current_setpoint - *current;

        switch( regulator_mode )
        {
            case REGULATOR_MODE_NONE:
            {
                regulator_state = REGULATOR_STATE_NONE;
            }

            case REGULATOR_MODE_SPEED:
            {
                //regulator_state = REGULATOR_STATE_CURRENT_LIMIT;
                //regulator_state = REGULATOR_STATE_SPEED;
                if(
                    omega_error > 0.0 * omega_setpoint &&
                    current_error <= 0.0 &&
                    regulator_state != REGULATOR_STATE_CURRENT_LIMIT
                )
                {
                    current_error_sum = (output_coeff - current_kp * current_error) / current_ki;
                    regulator_state = REGULATOR_STATE_CURRENT_LIMIT;
                }
                else if(
                    (abs(omega_error) < 0.01 * omega_setpoint ||
                    current_error > 0.01 * current_setpoint) &&
                    regulator_state != REGULATOR_STATE_SPEED
                )
                {
                    omega_error_sum = (output_coeff - omega_kp * omega_error) / omega_ki;
                    regulator_state = REGULATOR_STATE_SPEED;
                }
                break;
            }
        }

        // Vea numbriline integreerimine e. summeerimine
        if( regulator_state == REGULATOR_STATE_CURRENT_LIMIT )
        {
            current_error_sum = current_error_sum + current_error * loop_data->h;
            if(current_error_sum > 10000) current_error_sum = 10000;
            if(current_error_sum < -10000) current_error_sum = -10000;
        }

        if( regulator_state == REGULATOR_STATE_SPEED )
        {
            omega_error_sum = omega_error_sum + omega_error * loop_data->h;
            if(omega_error_sum > 10000) omega_error_sum = 10000;
            if(omega_error_sum < -10000) omega_error_sum = -10000;
        }

        // Current limit now?
        switch( regulator_state )
        {
            case REGULATOR_STATE_NONE:
            {
                output_coeff = 1.0;
                break;
            }

            case REGULATOR_STATE_CURRENT_LIMIT:
            {
                output_coeff = (current_kp * current_error) + (current_ki * current_error_sum) + current_kd * (current_error - current_error_old);
                break;
            }

            case REGULATOR_STATE_SPEED:
            {
                output_coeff = (omega_kp * omega_error) + (omega_ki * omega_error_sum) + omega_kd * (omega_error - omega_error_old);
                break;
            }
        }

        current_error_old = current_error;
        omega_error_old = omega_error;

        if(output_coeff > 1.0) output_coeff = 1.0;
        if(output_coeff < 0.0) output_coeff = 0.0;

        // Toitemuunduri mudel - väljundpinge arvutamine
        if( output_coeff * Un < kmPhi * *omega )
        {
            U = kmPhi * *omega;
        }
        else
        {
            U = output_coeff * Un;
        }

        // Toitemuunduri mudel - voolu piiramine
        // Voolu piirang regulaatoriga/dioodiga
        //if(*current > Imax) *current = Imax;

        // vool ühesuunaline - elektrilist pidurdust ei ole
        //if (i < 0) i=0;
        //if(*current < -Imax) *current = -Imax;

        // Calculate rheostat resistance
        switch( rheostat_mode )
        {
            case RHEOSTAT_MODE_CURRENT:
            {
                Rh = rheostat_current_model( *current, *omega );
                break;
            }

            case RHEOSTAT_MODE_SPEED:
            {
                Rh = rheostat_speed_model( *current, *omega );
                break;
            }

            default:
            {
                Rh = Ra;
                break;
            }
        }

        //
        //
        //

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
            // Time
            g_ascii_dtostr( loop_data->tmp_buf, 50, loop_data->t );
            g_string_append( loop_data->out, loop_data->tmp_buf );
            g_string_append( loop_data->out, "\t" );

            g_ascii_dtostr( loop_data->tmp_buf, 50, 520 );
            g_string_append( loop_data->out, loop_data->tmp_buf );
            g_string_append( loop_data->out, "\t" );

            // Current
            g_ascii_dtostr( loop_data->tmp_buf, 50, loop_data->y[0] );
            g_string_append( loop_data->out, loop_data->tmp_buf );
            g_string_append( loop_data->out, "\t" );

            // Angular speed
            g_ascii_dtostr( loop_data->tmp_buf, 50, loop_data->y[1] / ( 2*M_PI / 60 ) );
            g_string_append( loop_data->out, loop_data->tmp_buf );
            g_string_append( loop_data->out, "\t" );

            // Output coeff
            g_ascii_dtostr( loop_data->tmp_buf, 50, output_coeff );
            g_string_append( loop_data->out, loop_data->tmp_buf );
            g_string_append( loop_data->out, "\t" );

            // State
            g_string_append( loop_data->out, (regulator_state == REGULATOR_STATE_CURRENT_LIMIT ? "1" : "0") );
            g_string_append( loop_data->out, "\n" );

            g_output_stream_printf( G_OUTPUT_STREAM(loop_data->stream), NULL, NULL, NULL, "%s", loop_data->out->str );
            //printf( "%s", out->str );
            g_string_set_size( loop_data->out, 0 );

            PLFLT x = (PLFLT) loop_data->t;
            PLFLT y1 = (PLFLT) loop_data->y[0];
            PLFLT y2 = (PLFLT) loop_data->y[1];
            PLFLT y3 = (PLFLT) output_coeff;

            g_array_append_vals( x_array, &x, 1 );
            g_array_append_vals( y1_array, &y1, 1 );
            g_array_append_vals( y2_array, &y2, 1 );
            g_array_append_vals( y3_array, &y3, 1 );

			//time_print = t;
        }

        if( loop_data->i_max < loop_data->y[0] ) loop_data->i_max = loop_data->y[0];

        //usleep( 20*1000 );
    }

    //
    // Calculations finished
    //

    g_idle_add( dc_motor_stop, user_data );

    return NULL;
}

gboolean dc_motor_stop( gpointer user_data )
{
    struct CallbackObjekt *obj = (struct CallbackObjekt *) user_data;
    struct DC_MOTOR_LOOP_DATA *loop_data = obj->loop_data;

    gboolean aborted = (obj->dc_motor_run == FALSE ? TRUE : FALSE);

    loop_data->done = TRUE;
    g_thread_join( obj->pthread );

    g_source_remove( loop_data->event_source_timeout );

    g_free( loop_data->tmp_buf );
    g_string_free( loop_data->out, TRUE );
    g_string_free( loop_data->buffer, TRUE );

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
    obj->loop_data = NULL;
    obj->dc_motor_run = FALSE;

    gtk_button_set_label( button, "Calculate" );

    if( aborted == FALSE )
    {
        show_action_infobar( obj, "DC motor calculated and data is save into file!" );
        messagebox( obj->builder, "DC motor calculated and data is save into file!" );
    }
    else
    {
        messagebox( obj->builder, "DC motor calculation aborted!" );
    }

    return FALSE;
}

// #############################################################################
