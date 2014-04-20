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

#define copper_alpha    0.003862 // α = 0.003862 K¯¹
float Ra = 0.932 * ( 1 + copper_alpha * ( 20 - 120 ) ); // R( θ₀ ) = 120 °C, R( θ ) = R( θ₀ ) · ( 1 + α · Δθ ), Δθ = θ₂ - θ₁
float La = 11.5 / 1000; // ?!
float Jsum = 2.44 * ( 1 + 2 ); // sumaarne inertsimoment
float Pn = 36 * 1000; // W
float Nn = 710;  // p/min
float Un = 520;
const float kmPhi = 5.7619047619;

float Dt=0.00001;

// #############################################################################

gboolean dc_motor_init( struct CallbackObjekt *obj )
{

    gtk_label_printf( obj, "label_motor_type", "%s", motor_type );
    gtk_label_printf( obj, "label_mechanical_power", "%g kW", Pn / 1000 );
    gtk_label_printf( obj, "label_armature_resistance", "%g Ω", Ra );

    return TRUE;
}

// #############################################################################

float mechanical_load( float Omega )
{
	float Omega_n, Tn, Tkoormus;

	// Mootori nimikiirus, rad/s
	Omega_n = Nn * ( 2*M_PI / 60 );

	// nimimoment
	Tn = Pn / Omega_n;

	// Koormuse varinat a ) - konstantne
	Tkoormus = 0.7 * Tn;

	// Koormuse varinat b ) - ventilaatorkoormus
	//Tkoormus = 0.7 * Tn * pow( Omega / Omega_n, 2 );

	return Tkoormus;
}

// #############################################################################

void mechanical_model( float Te, float *omega )
{
    // J·(dω / dt) = Tm - Ts
    // Tm = kΦ·i
    // Ts = f(ω)
    // ω = dφ / dt

    // y[n+1] = y[n] + h · f(t[n], y[n])
    // ω[n+1] = ω[n] + h · f(t[n], ω[n])
    // ω[n+1] = ω[n] + h · ( kΦ·i - Ts(ω[n]) ) / J

    // ω[n+1] = i[n] + h · f(t[n] + h, ω[n+1])
    // ω[n+1] = ω[n] + h · ( kΦ·i - Ts(ω[n+1]) ) / J

     float Teff = Te - mechanical_load( *omega );
     *omega = *omega + Teff * Dt / Jsum;
}

// #############################################################################

void electrical_model( float Ua, float *ia, float Omega )
{
    // U = i·R + L (di / dt) + E
    // E = kΦ·ω

    // y[n+1] = y[n] + h · f(t[n], y[n])
    // i[n+1] = i[n] + h · f(t[n], i[n]) -- forward Euler method
    // i[n+1] = i[n] + h · ((U - i[n]·R - kΦ·ω) / L)

    // i[n+1] = i[n] + h · f(t[n] + h, i[n+1]) -- backward Euler method
    // i[n+1] = i[n] + h · ((U - i[n+1]·R - kΦ·ω) / L)
    // i[n+1] = i[n] + (h·U - h·i[n+1]·R - h·kΦ·ω) / L
    // L·i[n+1] = L·i[n] + h·U - h·i[n+1]·R - h·kΦ·ω
    // L·i[n+1] + h·i[n+1]·R = L·i[n] + h·U - h·kΦ·ω
    // i[n+1] · (L + h·R) = L·i[n] + h·U - h·kΦ·ω
    // i[n+1] = (L·i[n] + h·U - h·kΦ·ω) / (L + h·R)

    float t = 0;
    float E = kmPhi * Omega;

#define FFD_FUNCTION(t,y)   y1 + ( Ua - y * Ra - E ) * Dt / La
#define BDF_FUNCTION(t,y)   (La*y + Dt*Ua - Dt*E) / (La + Dt*Ra)

    float y1 = *ia; // y[n]
    float y2 = BDF_FUNCTION(t, y1); // y[n+1]

    y2 = FFD_FUNCTION(t, y2);

#undef FFD_FUNCTION
#undef BDF_FUNCTION

    *ia = y2;
}

// #############################################################################

gboolean dc_motor( struct CallbackObjekt *obj, GFileOutputStream *stream )
{

    float t = 0;
    float time_print = 0;
    float omega = 0, i = 0;
    float U = Un;
    float imax = 0;

    // Buffer for serialization with decimal point '.'
    GString *out = g_string_new( NULL );
    gchar *tmp_buf = g_malloc( 50 );

    // File header
    g_output_stream_printf( stream, "# t \t U \t i \t omega\n" );
    printf( "# t \t U \t i \t omega\n" );

    for( t = 0; t < 2; t = t + Dt )
    {
        electrical_model( U, &i, omega );
        mechanical_model( i * kmPhi, &omega );
        if( ( time_print + 0.001 ) < t )
        {
            g_ascii_dtostr( tmp_buf, 50, t );
            g_string_append( out, tmp_buf );
            g_string_append( out, "\t" );

            g_ascii_dtostr( tmp_buf, 50, U );
            g_string_append( out, tmp_buf );
            g_string_append( out, "\t" );

            g_ascii_dtostr( tmp_buf, 50, i );
            g_string_append( out, tmp_buf );
            g_string_append( out, "\t" );

            g_ascii_dtostr( tmp_buf, 50, omega / ( 2*M_PI / 60 ) );
            g_string_append( out, tmp_buf );
            g_string_append( out, "\n" );

            g_output_stream_printf( stream, "%s", out->str );
            printf( "%s", out->str );
            g_string_set_size( out, 0 );
			//g_output_stream_printf( stream, "%.4f\t%.0f\t%.0f\t%.0f\n", t, U, i, omega / ( 2*M_PI / 60 ) );
			//printf( "%g\t%g\t%g\t%g\n", t, U, i, omega / ( 2*M_PI / 60 ) );

			time_print = t;
        }

        if( imax < i ) imax = i;
    }

    gtk_label_printf( obj, "label_starting_current", "%g A", imax );

    g_free( tmp_buf );
    g_string_free( out, TRUE );

    return TRUE;
}

// #############################################################################
