/*
 * linregmodel.c
 *
 *  Created on: 2014/05/19
 *      Author: utsugi
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <linregmodel.h>

#include "private.h"

/* centering each column of matrix:
 * x(:, j) -> x(:, j) - mean(x(:, j)) */
static void
do_centering (mm_dense *x)
{
	int		j;
	for (j = 0; j < x->n; j++) {
		int		i;
		double	meanj = 0.;
		for (i = 0; i < x->m; i++) meanj += x->data[i + j * x->m];
		meanj /= (double) x->m;
		for (i = 0; i < x->m; i++) x->data[i + j * x->m] -= meanj;
	}
	return;
}

/* normalizing each column of matrix:
 * x(:, j) -> x(:, j) / norm(x(:, j)) */
static void
do_normalizing (mm_real *x)
{
	int		j;
	for (j = 0; j < x->n; j++) {
		double	alpha;
		double	nrmj;
		int		size = (mm_real_is_sparse (x)) ? x->p[j + 1] - x->p[j] : x->m;
		double	*xj = x->data + ((mm_real_is_sparse (x)) ? x->p[j] : j * x->m);
		nrmj = dnrm2_ (&size, xj, &ione);
		alpha = 1. / nrmj;
		dscal_ (&size, &alpha, xj, &ione);
	}
	return;
}

/* allocate linregmodel object */
static linregmodel *
linregmodel_alloc (void)
{
	linregmodel	*lreg = (linregmodel *) malloc (sizeof (linregmodel));
	lreg->has_copy = false;
	lreg->y = NULL;
	lreg->x = NULL;
	lreg->d = NULL;
	lreg->lambda2 = 0.;
	lreg->is_regtype_lasso = true;

	lreg->c = NULL;
	lreg->logcamax = 0.;

	lreg->ycentered = false;
	lreg->xcentered = false;
	lreg->xnormalized = false;

	lreg->sy = 0.;
	lreg->sx = NULL;
	lreg->xtx = NULL;
	lreg->dtd = NULL;

	return lreg;
}

/* create new linregmodel object */
linregmodel *
linregmodel_new (mm_dense *y, mm_real *x, const double lambda2, mm_real *d, bool has_copy, PreProc proc)
{
	double			camax;
	linregmodel	*lreg;

	if (!y) error_and_exit ("linregmodel_new", "y is empty.", __FILE__, __LINE__);
	if (!x) error_and_exit ("linregmodel_new", "x is empty.", __FILE__, __LINE__);
	if (!mm_real_is_dense (y)) error_and_exit ("linregmodel_new", "y must be dense.", __FILE__, __LINE__);
	if (y->n != 1) error_and_exit ("linregmodel_new", "y must be vector.", __FILE__, __LINE__);
	if (y->m != x->m) error_and_exit ("linregmodel_new", "dimensions of matrix x and vector y do not match.", __FILE__, __LINE__);
	if (d && x->n != d->n) error_and_exit ("linregmodel_new", "dimensions of matrix x and d do not match.", __FILE__, __LINE__);

	lreg = linregmodel_alloc ();

	/* has copy y, x and d ? */
	lreg->has_copy = has_copy;
	if (has_copy) {	// copy y, x and d
		lreg->y = mm_real_copy (y);
		lreg->x = mm_real_copy (x);
		if (d) lreg->d = mm_real_copy (d);
	} else {	// only store pointers
		lreg->x = x;
		lreg->y = y;
		lreg->d = d;
	}

	/* lambda2 */
	if (lambda2 > DBL_EPSILON) lreg->lambda2 = lambda2;

	/* if lambda2 > 0 && d != NULL, regression type is NOT lasso: is_regtype_lasso = false */
	if (lreg->lambda2 > DBL_EPSILON && lreg->d) lreg->is_regtype_lasso = false;

	/* centering y */
	if (proc & DO_CENTERING_Y) {
		do_centering (lreg->y);
		lreg->ycentered = true;
	}
	/* standardizing x */
	if (proc & DO_CENTERING_X) {
		if (mm_real_is_sparse (lreg->x)) {
			mm_real	*tmp = lreg->x;
			lreg->x = mm_real_sparse_to_dense (tmp);
			if (lreg->has_copy) mm_real_free (tmp);
		}
		do_centering (lreg->x);
		lreg->xcentered = true;
	}
	if (proc & DO_NORMALIZING_X) {
		do_normalizing (lreg->x);
		lreg->xnormalized = true;
	}

	// c = X' * y
	lreg->c = mm_real_x_dot_y (true, 1., lreg->x, lreg->y, 0.);

	// camax = max ( abs (c) )
	camax = fabs (lreg->c->data[idamax_ (&lreg->c->nz, lreg->c->data, &ione) - 1]);
	lreg->logcamax = floor (log10 (camax)) + 1.;

	/* sum y */
	if (!lreg->ycentered) lreg->sy = mm_real_xj_sum (0, lreg->y);

	/* sx(j) = sum X(:,j) */
	if (!lreg->xcentered) {
		int		j;
		lreg->sx = (double *) malloc (lreg->x->n * sizeof (double));
		for (j = 0; j < lreg->x->n; j++) lreg->sx[j] = mm_real_xj_sum (j, lreg->x);
	}

	/* xtx = diag (X' * X) */
	if (!lreg->xnormalized) {
		int		j;
		lreg->xtx = (double *) malloc (lreg->x->n * sizeof (double));
		for (j = 0; j < lreg->x->n; j++) lreg->xtx[j] = pow (mm_real_xj_nrm2 (j, lreg->x), 2.);
	}

	/* dtd = diag (D' * D) */
	if (!lreg->is_regtype_lasso) {
		int		j;
		lreg->dtd = (double *) malloc (lreg->d->n * sizeof (double));
		for (j = 0; j < lreg->d->n; j++) lreg->dtd[j] = pow (mm_real_xj_nrm2 (j, lreg->d), 2.);
	}

	return lreg;
}

/* destroy linregmodel object */
void
linregmodel_free (linregmodel *lreg)
{
	if (lreg) {
		if (lreg->has_copy) {
			if (lreg->y) mm_real_free (lreg->y);
			if (lreg->x) mm_real_free (lreg->x);
			if (lreg->d) mm_real_free (lreg->d);
		}
		if (lreg->sx) free (lreg->sx);
		if (lreg->xtx) free (lreg->xtx);
		if (lreg->dtd) free (lreg->dtd);
		free (lreg);
	}
	return;
}
