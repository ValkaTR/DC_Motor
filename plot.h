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

// #############################################################################

#ifndef PLOT_H_INCLUDED
#define PLOT_H_INCLUDED

// #############################################################################

enum DISPLAY_CHARACTERISTIC
{
    DISPLAY_NONE = 0,
    DISPLAY_CURRENT,
    DISPLAY_SPEED,
    DISPLAY_OUTPUT_COEFF,
    DISPLAY_COUNT
};

// #############################################################################

extern PLFLT time_xmax;

// #############################################################################

gboolean plot_init( struct CallbackObjekt *obj );
gboolean plot_deinit( struct CallbackObjekt *obj );

G_MODULE_EXPORT gboolean drawingarea1_draw_cb( GtkWidget *zeichenflaeche, cairo_t *cr, struct CallbackObjekt *obj );

// #############################################################################

#endif // PLOT_H_INCLUDED

// #############################################################################
