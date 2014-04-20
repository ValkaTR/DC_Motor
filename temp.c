// #############################################################################

// Linear system solve test

int
linsys_solve (void)
{
  double a_data[] = { 0.18, 0.60, 0.57, 0.96,
                      0.41, 0.24, 0.99, 0.58,
                      0.14, 0.30, 0.97, 0.66,
                      0.51, 0.13, 0.19, 0.85 };

  double b_data[] = { 1.0, 2.0, 3.0, 4.0 };

  gsl_matrix_view m = gsl_matrix_view_array (a_data, 4, 4);
  gsl_vector_view b = gsl_vector_view_array (b_data, 4);

  gsl_vector *x = gsl_vector_alloc (4);

  int s;

  gsl_permutation * p = gsl_permutation_alloc (4);

  gsl_linalg_LU_decomp (&m.matrix, p, &s);

  gsl_linalg_LU_solve (&m.matrix, p, &b.vector, x);

  printf ("x = \n");
  gsl_vector_fprintf (stdout, x, "%g");

  gsl_permutation_free (p);
  gsl_vector_free (x);
  return 0;
}

// #############################################################################

// ODE Test

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
int func (double t, const double y[], double f[], void *params)
{
    double mu = *(double *) params;
    f[0] = y[1];
    f[1] = -y[0] - mu*y[1]*(y[0]*y[0] - 1);

    return GSL_SUCCESS;
}

int ode_test( )
{
    const gsl_odeiv2_step_type *step_type = gsl_odeiv2_step_rk8pd;
    const double eps_abs = 1e-6;
    const double eps_rel = 0;

    gsl_odeiv2_step *ode_step = gsl_odeiv2_step_alloc( step_type, 2 );
    gsl_odeiv2_control *ode_control = gsl_odeiv2_control_y_new( eps_abs, eps_rel );
    gsl_odeiv2_evolve *ode_evolve = gsl_odeiv2_evolve_alloc( 2 );

    double mu = 10;
    gsl_odeiv2_system ode_sys = { func, NULL, 2, &mu };

    double t = 0.0, t1 = 100.0;
    double h = 1e-6;
    double y[2] = { 1.0, 0.0 };

    while( t < t1 )
    {
        int status = gsl_odeiv2_evolve_apply(
            ode_evolve, ode_control, ode_step, &ode_sys,
            &t,     /* System time (input, output) */
            t1,     /* Maximum time (input) */
            &h,     /* Step-size (input, output) */
            y       /* System position (input, output) */
        );

        if( status != GSL_SUCCESS )
            break;

        printf( "%.5e %.5e %.5e\n", t, y[0], y[1] );
    }

    gsl_odeiv2_evolve_free( ode_evolve );
    gsl_odeiv2_control_free( ode_control );
    gsl_odeiv2_step_free( ode_step );
    return 0;
}

// #############################################################################
