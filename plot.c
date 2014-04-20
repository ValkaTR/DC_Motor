// #############################################################################

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
#include "plot.h"

// #############################################################################

GArray *x_array = NULL;
GArray *y1_array = NULL;
GArray *y2_array = NULL;
GArray *y3_array = NULL;

enum DISPLAY_CHARACTERISTIC display_characteristic = DISPLAY_NONE;

PLFLT time_xmin = 0.0, time_xmax = 10.0;
PLFLT current_ymin = 0.0, current_ymax = 200.0;
PLFLT speed_ymin = 0.0, speed_ymax = 80.0;
PLFLT output_coeff_ymin = 0.0, output_coeff_ymax = 1.1;

// #############################################################################

G_MODULE_EXPORT void combobox1_changed_cb( GtkComboBox *combo, struct CallbackObjekt *obj )
{
    GtkTreeIter iter;
    if( gtk_combo_box_get_active_iter( combo, &iter ) )
    {
        gint id;
        gchar *name;

        /* Auswahl besorgen */
        GtkTreeModel *model = GTK_TREE_MODEL( gtk_builder_get_object( obj->builder, "liststore1" ) );
        gtk_tree_model_get( model, &iter, 0, &id, 1, &name, -1 );

        /* Anzeige der Auswahl im Hauptfenster, Statuszeile */
        //g_printf( "[%i] %s\n", id, name );

        display_characteristic = (enum DISPLAY_CHARACTERISTIC) id;

        /* Aufräumen */
        g_free( name );
    }
}

// #############################################################################

static double t = 0.0;
gboolean timeout_test( struct CallbackObjekt *obj )
{
    t += 0.02;

    gint breite, hoehe;
    breite = gtk_widget_get_allocated_width( obj->drawingarea );
    hoehe = gtk_widget_get_allocated_height( obj->drawingarea );

    /* Zum Zeichnen auffordern */
    gtk_widget_queue_draw_area( obj->drawingarea, 0, 0, breite, hoehe );

    return TRUE;
}

// #############################################################################

gboolean plot_init( struct CallbackObjekt *obj )
{
    obj->drawingarea = GTK_WIDGET( gtk_builder_get_object( obj->builder, "drawingarea1" ) );
    g_timeout_add( 20 /* ms */, (GSourceFunc) timeout_test, obj );

    x_array = g_array_new( FALSE, FALSE, sizeof(PLFLT) );
    y1_array = g_array_new( FALSE, FALSE, sizeof(PLFLT) );
    y2_array = g_array_new( FALSE, FALSE, sizeof(PLFLT) );
    y3_array = g_array_new( FALSE, FALSE, sizeof(PLFLT) );

    // Initialize PLPlot
    //plparseopts( &argc, (const char **) argv, PL_PARSE_FULL );

    plsdev( "extcairo" );
    plinit( );

    return TRUE;
}

gboolean plot_deinit( struct CallbackObjekt *obj )
{
    plend( );

    g_array_free( x_array, TRUE );
    g_array_free( y1_array, TRUE );
    g_array_free( y2_array, TRUE );
    g_array_free( y3_array, TRUE );

    return TRUE;
}

// #############################################################################

G_MODULE_EXPORT gboolean drawingarea1_draw_cb( GtkWidget *zeichenflaeche, cairo_t *cr, struct CallbackObjekt *obj )
{
    double breite, hoehe;
    breite = gtk_widget_get_allocated_width( zeichenflaeche );
    hoehe = gtk_widget_get_allocated_height( zeichenflaeche );

    cairo_move_to( cr, 0.0, 0.0 );
    cairo_line_to( cr, breite, 0.0 );
    cairo_line_to( cr, breite, hoehe );
    cairo_line_to( cr, 0.0, hoehe );
    cairo_line_to( cr, 0.0, 0.0 );
    //cairo_line_to( cr, breite, hoehe );

    pl_cmd( PLESC_DEVINIT, cr );

    //plsdiori( 0.0 );
    //plspage( 90, 90, breite, hoehe, 0, 0);
    //plvsta( );
    //plptex( 0, 0, 0, 0, 0, "sdfsdf" );

    /** Set relative margin width, aspect ratio, and relative justification
     * that define current device-space window. */
    plsdidev(
        0.0,/** Relative margin width. */
        breite / hoehe, /** Aspect ratio. */
        0.5, /** Relative justification in x. Value must lie in the range -0.5 to 0.5. */
        0.0 /** Relative justification in y. Value must lie in the range -0.5 to 0.5. */
    );

    /** Integer representing the color. The defaults at present are (these may change):
     * 0 	black (default background)
     * 1 	red (default foreground)
     * 2 	yellow
     * 3 	green
     * 4 	aquamarine
     * 5 	pink
     * 6 	wheat
     * 7 	grey
     * 8 	brown
     * 9 	blue
     * 10 	BlueViolet
     * 11 	cyan
     * 12 	turquoise
     * 13 	magenta
     * 14 	salmon
     * 15 	white
     */
    plcol0( 0 );

    /**  Specifies the font:
     *
     *     1: Normal font (simplest and fastest)
     *
     *     2: Roman font
     *
     *     3: Italic font
     *
     *     4: Script font
     */
    plfont( 2 );

    /** Controls how the axes will be scaled:
     *
     *   -1: the scales will not be set, the user must set up the scale before
     *       calling plenv using plsvpa, plvasp or other.
     *
     *   0: the x and y axes are scaled independently to use as much of the
     *      screen as possible.
     *
     *   1: the scales of the x and y axes are made equal.
     *
     *   2: the axis of the x and y axes are made equal, and the plot box will
     *      be square.
     */
    PLINT just = 0;

    /** Controls drawing of the box around the plot:
     *
     * -2: draw no box, no tick marks, no numeric tick labels, no axes.
     *
     * -1: draw box only.
     *
     * 0: draw box, ticks, and numeric tick labels.
     *
     * 1: also draw coordinate axes at x=0 and y=0.
     *
     * 2: also draw a grid at major tick positions in both coordinates.
     *
     * 3: also draw a grid at minor tick positions in both coordinates.
     *
     * 10: same as 0 except logarithmic x tick marks.
     *     (The x data have to be converted to logarithms separately.)
     *
     * 11: same as 1 except logarithmic x tick marks.
     *     (The x data have to be converted to logarithms separately.)
     *
     * 12: same as 2 except logarithmic x tick marks.
     *     (The x data have to be converted to logarithms separately.)
     *
     * 13: same as 3 except logarithmic x tick marks.
     *     (The x data have to be converted to logarithms separately.)
     *
     * 20: same as 0 except logarithmic y tick marks.
     *     (The y data have to be converted to logarithms separately.)
     *
     * 21: same as 1 except logarithmic y tick marks.
     *     (The y data have to be converted to logarithms separately.)
     *
     * 22: same as 2 except logarithmic y tick marks.
     *     (The y data have to be converted to logarithms separately.)
     *
     * 23: same as 3 except logarithmic y tick marks.
     *     (The y data have to be converted to logarithms separately.)
     *
     * 30: same as 0 except logarithmic x and y tick marks.
     *     (The x and y data have to be converted to logarithms separately.)
     *
     * 31: same as 1 except logarithmic x and y tick marks.
     *     (The x and y data have to be converted to logarithms separately.)
     *
     * 32: same as 2 except logarithmic x and y tick marks.
     *     (The x and y data have to be converted to logarithms separately.)
     *
     * 33: same as 3 except logarithmic x and y tick marks.
     *     (The x and y data have to be converted to logarithms separately.)
     *
     * 40: same as 0 except date / time x labels.
     *
     * 41: same as 1 except date / time x labels.
     *
     * 42: same as 2 except date / time x labels.
     *
     * 43: same as 3 except date / time x labels.
     *
     * 50: same as 0 except date / time y labels.
     *
     * 51: same as 1 except date / time y labels.
     *
     * 52: same as 2 except date / time y labels.
     *
     * 53: same as 3 except date / time y labels.
     *
     * 60: same as 0 except date / time x and y labels.
     *
     * 61: same as 1 except date / time x and y labels.
     *
     * 62: same as 2 except date / time x and y labels.
     *
     * 63: same as 3 except date / time x and y labels.
     *
     * 70: same as 0 except custom x and y labels.
     *
     * 71: same as 1 except custom x and y labels.
     *
     * 72: same as 2 except custom x and y labels.
     *
     * 73: same as 3 except custom x and y labels.
     */
    PLINT axis = 1;

    //#define NSIZE    101
    //PLFLT x[NSIZE], y[NSIZE];
    PLFLT xmin = time_xmin, xmax = time_xmax, ymin = -1., ymax = 200.;

    switch( display_characteristic )
    {
        case DISPLAY_CURRENT: { ymin = current_ymin; ymax = current_ymax; break; }
        case DISPLAY_SPEED: { ymin = speed_ymin; ymax = speed_ymax; break; }
        case DISPLAY_OUTPUT_COEFF: { ymin = output_coeff_ymin; ymax = output_coeff_ymax; break; }
        case DISPLAY_NONE:
        default: { ymin = -1; ymax = 1; break; }
    }

    plenv( xmin, xmax, ymin, ymax, just, axis );

    pllab(
          "x", /** Label for horizontal axis. */
          "y", /** Label for vertical axis. */
          "title" /** Title of graph. */
    );

    // Prepare data to be plotted.
    //int i;
    //double f = 2;
    //double omega = 2 * M_PI * f;
    //for( i = 0; i < NSIZE; i++ )
    //{
    //    x[i] = 2 * M_PI * (PLFLT) (i) / (PLFLT) (NSIZE - 1);
    //    y[i] = sin( x[i] + omega * t );
    //}

    // Plot the data that was prepared above.
    //plline( NSIZE, x, y );
    switch( display_characteristic )
    {
        case DISPLAY_CURRENT:
        {
            plline( x_array->len, (const PLFLT *) x_array->data, (const PLFLT *) y1_array->data );
            break;
        }

        case DISPLAY_SPEED:
        {
            plline( x_array->len, (const PLFLT *) x_array->data, (const PLFLT *) y2_array->data );
            break;
        }

        case DISPLAY_OUTPUT_COEFF:
        {
            plline( x_array->len, (const PLFLT *) x_array->data, (const PLFLT *) y3_array->data );
            break;
        }

        case DISPLAY_NONE:
        default:
        {
            break;
        }
    }


    //const gchar *anzeigetext = "text";
    //cairo_text_extents_t ausdehnung;
    //cairo_set_source_rgb( cr, 1, 0, 1 );
    /* Mittelpunkt festlegen */
    //cairo_translate( cr, breite / 2.0, hoehe / 2.0 );
    /* Font vergrößern */
    //cairo_set_font_size( cr, 100 );
    /* Text rotieren lassen */
    //cairo_rotate( cr, 0.0 );
    /* Textausdehnung ermitteln */
    //cairo_text_extents( cr, anzeigetext, &ausdehnung );
    /* Anfangspunkt für Text verschieben */
    //cairo_move_to( cr, -ausdehnung.width / 2.0, ausdehnung.height / 2.0 );
    /* Text ausgeben */
    //cairo_show_text( cr, anzeigetext );

    return TRUE;
}

// #############################################################################
