/*
 * linreg.c
 *
 *  Created on: 2014/05/19
 *      Author: utsugi
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <linregmodel.h>

#include "private.h"

/* centering each column of matrix:
 * x(:, j) -> x(:, j) - mean(x(:, j)) */
static void
centering (mm_dense *x)
{
	int		i, j;
	for (j = 0; j < x->n; j++) {
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
normalizing (mm_real *x)
{
	int		j;
	for (j = 0; j < x->n; j++) {
		double	alpha;
		double	nrmj;
		int		size = (mm_is_sparse (x->typecode)) ? x->p[j + 1] - x->p[j] : x->m;
		double	*xj = x->data + ((mm_is_sparse (x->typecode)) ? x->p[j] : j * x->m);
		nrmj = dnrm2_ (&size, xj, &ione);
		alpha = 1. / nrmj;
		dscal_ (&size, &alpha, xj, &ione);
	}
	return;
}

linregmodel *
linregmodel_alloc (void)
{
	linregmodel	*lreg = (linregmodel *) malloc (sizeof (linregmodel));
	lreg->has_copy = false;
	lreg->y = NULL;
	lreg->x = NULL;
	lreg->d = NULL;
	lreg->lambda2 = 0.;

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

linregmodel *
linregmodel_new (mm_real *y, mm_real *x, const double lambda2, mm_real *d, bool has_copy, bool ycentering, bool xcentering, bool xnormalizing)
{
	double			camax;
	linregmodel	*lreg;

	if (!y) cdescent_error ("linregmodel_new", "vector *y is empty.", __FILE__, __LINE__);
	if (!x) cdescent_error ("linregmodel_new", "matrix *x is empty.", __FILE__, __LINE__);
	if (!mm_is_dense (y->typecode)) cdescent_error ("linregmodel_new", "vector *y must be dense.", __FILE__, __LINE__);
	if (y->m != x->m) cdescent_error ("linregmodel_new", "size of matrix *x and vector *y are not match.", __FILE__, __LINE__);
	if (d && x->n != d->n) cdescent_error ("linregmodel_new", "size of matrix *x and *d are not match.", __FILE__, __LINE__);

	lreg = linregmodel_alloc ();

	lreg->has_copy = has_copy;
	if (has_copy) {
		lreg->y = mm_real_copy (y);
		lreg->x = mm_real_copy (x);
		lreg->d = (d) ? mm_real_copy (d) : NULL;
	} else {
		lreg->x = x;
		lreg->y = y;
		lreg->d = d;
	}
	lreg->lambda2 = (lambda2 > cdescent_double_eps ()) ? lambda2 : 0.;

	if (ycentering) {
		if (mm_is_sparse (lreg->y->typecode)) 	mm_real_replace_sparse_to_dense (lreg->y);
		centering (lreg->y);
		lreg->ycentered = true;
	}
	if (xcentering) {
		if (mm_is_sparse (lreg->x->typecode)) 	mm_real_replace_sparse_to_dense (lreg->x);
		centering (lreg->x);
		lreg->xcentered = true;
	}
	if (xnormalizing) {
		normalizing (lreg->x);
		lreg->xnormalized = true;
	}

	// c = X' * y
	lreg->c = mm_real_x_dot_y (true, 1., lreg->x, lreg->y, 0.);

	// camax = max ( abs (c) )
	camax = fabs (lreg->c->data[idamax_ (&lreg->c->nz, lreg->c->data, &ione) - 1]);
	lreg->logcamax = floor (log10 (camax)) + 1.;

	/* sum y */
	if (!lreg->ycentered) lreg->sy = mm_real_sum (lreg->y);

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
	if (!linregmodel_is_regtype_lasso (lreg)) {
		int				j;
		lreg->dtd = (double *) malloc (lreg->d->n * sizeof (double));
		for (j = 0; j < lreg->d->n; j++) lreg->dtd[j] = pow (mm_real_xj_nrm2 (j, lreg->d), 2.);
	}

	return lreg;
}

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

/* if lambda2 < eps || d == NULL, regression type = lasso */
bool
linregmodel_is_regtype_lasso (const linregmodel *lreg)
{
	return (lreg->lambda2 < cdescent_double_eps () || lreg->d == NULL);
}