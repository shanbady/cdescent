/*
 * linregmodel.c
 *
 *  Created on: 2014/05/19
 *      Author: utsugi
 */

#include <stdlib.h>
#include <math.h>
#include <linregmodel.h>

#include "private/private.h"

/* calculate sum x(:,j) */
static bool
calc_sum (const mm_real *x, double **sum)
{
	int		j;
	bool	centered;
	double	*_sum = (double *) malloc (x->n * sizeof (double));
	for (j = 0; j < x->n; j++) _sum[j] = mm_real_xj_sum (x, j);
	// check whether mean is all 0 (x is already centered)
	centered = true;
	for (j = 0; j < x->n; j++) {
		if (fabs (_sum[j] / (double) x->m) > SQRT_DBL_EPSILON) {
			centered = false;
			break;
		}
	}
	if (centered) {	// mean is all 0
		*sum = NULL;
		free (_sum);
	} else *sum = _sum;

	return centered;
}

/* calculate sum x(:,j)^2 */
static bool
calc_ssq (const mm_real *x, double **ssq)
{
	int		j;
	bool	normalized;
	double	*_ssq = (double *) malloc (x->n * sizeof (double));
	for (j = 0; j < x->n; j++) _ssq[j] = mm_real_xj_ssq (x, j);
	// check whether norm is all 1 (x is already normalized)
	normalized = true;
	for (j = 0; j < x->n; j++) {
		if (fabs (_ssq[j] - 1.) > SQRT_DBL_EPSILON) {
			normalized = false;
			break;
		}
	}
	if (normalized) {
		*ssq = NULL;	// norm is all 1
		free (_ssq);
	} else *ssq = _ssq;

	return normalized;
}

/* centering each column of matrix:
 * x(:,j) -> x(:,j) - mean(x(:,j)) */
static void
do_centering (mm_dense *x, const double *sum)
{
	int		j;
	for (j = 0; j < x->n; j++) {
		int		i;
		double	meanj = sum[j] / (double) x->m;
		for (i = 0; i < x->m; i++) x->data[i + j * x->m] -= meanj;
	}
	return;
}

/* normalizing each column of matrix:
 * x(:,j) -> x(:,j) / norm(x(:,j)) */
static void
do_normalizing (mm_real *x, const double *ssq)
{
	int		j;
	for (j = 0; j < x->n; j++) {
		double	nrm2j = sqrt (ssq[j]);
		if (nrm2j > SQRT_DBL_EPSILON) {
			double	alpha = 1. / nrm2j;
			int		size = (mm_real_is_sparse (x)) ? x->p[j + 1] - x->p[j] : x->m;
			double	*xj = x->data + ((mm_real_is_sparse (x)) ? x->p[j] : j * x->m);
			dscal_ (&size, &alpha, xj, &ione);
		}
	}
	return;
}

/* allocate linregmodel object */
static linregmodel *
linregmodel_alloc (void)
{
	linregmodel	*lreg = (linregmodel *) malloc (sizeof (linregmodel));
	if (lreg == NULL) return NULL;

	lreg->has_copy_y = false;
	lreg->has_copy_x = false;

	lreg->y = NULL;
	lreg->x = NULL;
	lreg->d = NULL;
	lreg->lambda2 = 0.;
	lreg->is_regtype_lasso = true;

	lreg->c = NULL;
	lreg->log10camax = 0.;

	lreg->ycentered = false;
	lreg->xcentered = false;
	lreg->xnormalized = false;

	lreg->sy = NULL;
	lreg->sx = NULL;
	lreg->xtx = NULL;
	lreg->dtd = NULL;

	return lreg;
}

/*** create new linregmodel object
 * INPUT:
 * mm_dense          *y: dense vector
 * mm_sparse         *x: sparse general / symmetric matrix
 * const double lambda2: regularization parameter
 * const mm_real     *d: general linear operator of penalty
 * PreProc         proc: specify pre-processing for y and x
 *                       DO_CENTERING_Y: do centering of y
 *                       DO_CENTERING_X: do centering of each column of x
 *                       DO_NORMALIZING_X: do normalizing of each column of x
 *                       DO_STANDARDIZING_X: do centering and normalizing of each column of x ***/
linregmodel *
linregmodel_new (mm_dense *y, mm_real *x, const double lambda2, const mm_real *d, PreProc proc)
{
	double			camax;
	linregmodel	*lreg;

	/* check whether x and y are not empty */
	if (!y) error_and_exit ("linregmodel_new", "y is empty.", __FILE__, __LINE__);
	if (!x) error_and_exit ("linregmodel_new", "x is empty.", __FILE__, __LINE__);

	/* check whether lambda2 >= 0. */
	if (lambda2 < 0.) error_and_exit ("linregmodel_new", "lambda2 must be >= 0.", __FILE__, __LINE__);

	/* check whether y is dense asymmetric vector */
	if (!mm_real_is_dense (y)) error_and_exit ("linregmodel_new", "y must be dense.", __FILE__, __LINE__);
	if (mm_real_is_symmetric (y)) error_and_exit ("linregmodel_new", "y must be general.", __FILE__, __LINE__);
	if (y->n != 1) error_and_exit ("linregmodel_new", "y must be vector.", __FILE__, __LINE__);

	/* if x or d is symmetric, check whether it is square */
	if (mm_real_is_symmetric (x) && x->m != x->n) error_and_exit ("linregmodel_new", "symmetric matrix must be square.", __FILE__, __LINE__);
	if (d && mm_real_is_symmetric (d) && d->m != d->n) error_and_exit ("linregmodel_new", "symmetric matrix must be square.", __FILE__, __LINE__);

	/* check dimensions of vector and matrix */
	if (y->m != x->m) error_and_exit ("linregmodel_new", "dimensions of matrix x and vector y do not match.", __FILE__, __LINE__);
	if (d && x->n != d->n) error_and_exit ("linregmodel_new", "dimensions of matrix x and d do not match.", __FILE__, __LINE__);

	lreg = linregmodel_alloc ();
	if (lreg == NULL) error_and_exit ("linregmodel_new", "failed to allocate object.", __FILE__, __LINE__);

	/* lambda2 */
	if (lambda2 > 0.) lreg->lambda2 = lambda2;

	lreg->y = y;	// in initial, has_copy_y = false
	lreg->ycentered = calc_sum (lreg->y, &lreg->sy);
	/* If DO_CENTERING_Y is set and y is not already centered */
	if ((proc & DO_CENTERING_Y) && !lreg->ycentered) {
		lreg->y = mm_real_copy (y);
		lreg->has_copy_y = true;
		do_centering (lreg->y, lreg->sy);
		lreg->ycentered = true;
	}

	lreg->x = x;	// in initial, has_copy_x = false
	lreg->xcentered = calc_sum (lreg->x, &lreg->sx);
	/* If DO_CENTERING_X is set and x is not already centered */
	if ((proc & DO_CENTERING_X) && !lreg->xcentered) {
		/* if lreg->x is sparse, convert to dense matrix */
		if (mm_real_is_sparse (lreg->x)) {
			mm_sparse	*tmp = lreg->x;
			lreg->x = mm_real_sparse_to_dense (tmp);
			if (lreg->has_copy_x == true) mm_real_free (tmp);
			else lreg->has_copy_x = true;
		}
		/* if lreg->x is symmetric, convert to general matrix */
		if (mm_real_is_symmetric (lreg->x)) {
			mm_real	*tmp = lreg->x;
			lreg->x = mm_real_symmetric_to_general (tmp);
			if (lreg->has_copy_x == true) mm_real_free (tmp);
			else lreg->has_copy_x = true;
		}
		if (!lreg->has_copy_x) {
			lreg->x = mm_real_copy (x);
			lreg->has_copy_x = true;
		}
		do_centering (lreg->x, lreg->sx);
		lreg->xcentered = true;
	}

	lreg->xnormalized = calc_ssq (lreg->x, &lreg->xtx);
	/* If DO_NORMALIZING_X is set and x is not already normalized */
	if ((proc & DO_NORMALIZING_X) && !lreg->xnormalized) {
		/* if lreg->x is symmetric, convert to general matrix */
		if (mm_real_is_symmetric (lreg->x)) {
			mm_real	*tmp = lreg->x;
			lreg->x = mm_real_symmetric_to_general (tmp);
			if (lreg->has_copy_x == true) mm_real_free (tmp);
			else lreg->has_copy_x = true;
		}
		do_normalizing (lreg->x, lreg->xtx);
		lreg->xnormalized = true;
	}

	if (d) lreg->d = mm_real_copy (d);
	/* if lambda2 > 0 && d != NULL,
	 * regression type is NOT lasso: is_regtype_lasso = false */
	if (lreg->lambda2 > 0. && lreg->d) lreg->is_regtype_lasso = false;
	/* dtd = diag (D' * D) */
	if (!lreg->is_regtype_lasso) {
		int		j;
		lreg->dtd = (double *) malloc (lreg->d->n * sizeof (double));
		for (j = 0; j < lreg->d->n; j++) lreg->dtd[j] = mm_real_xj_ssq (lreg->d, j);
	}

	// c = X' * y
	lreg->c = mm_real_x_dot_y (true, 1., lreg->x, lreg->y);

	// camax = max ( abs (c) )
	camax = fabs (lreg->c->data[idamax_ (&lreg->c->nz, lreg->c->data, &ione) - 1]);
	lreg->log10camax = floor (log10 (camax)) + 1.;

	return lreg;
}

/*** free linregmodel object ***/
void
linregmodel_free (linregmodel *lreg)
{
	if (lreg) {
		if (lreg->y && lreg->has_copy_y) mm_real_free (lreg->y);
		if (lreg->x && lreg->has_copy_x) mm_real_free (lreg->x);
		if (lreg->d) mm_real_free (lreg->d);
		if (lreg->sy) free (lreg->sy);
		if (lreg->sx) free (lreg->sx);
		if (lreg->xtx) free (lreg->xtx);
		if (lreg->dtd) free (lreg->dtd);
		free (lreg);
	}
	return;
}
