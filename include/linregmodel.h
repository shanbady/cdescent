/*
 * linregmodel.h
 *
 *  linear regression model
 *
 *  Created on: 2014/04/08
 *      Author: utsugi
 */

#ifndef LINREGMODEL_H_
#define LINREGMODEL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <mmreal.h>

/* flag of data preprocessing */
typedef enum {
	DO_NOTHING       = 0x0,		// do nothing
	DO_CENTERING_Y   = 1 << 0,	// do centering y
	DO_CENTERING_X   = 1 << 1,	// do centering x
	DO_NORMALIZING_X = 1 << 2,	// do normalizing x
	// do standardizing x
	DO_STANDARDIZING_X = DO_CENTERING_X | DO_NORMALIZING_X
} PreProc;

/*** Object of convex/nonconvex regularized linear regression problem
 *
 *   argmin_beta || b - Z * beta ||^2
 *
 *   where
 *   	b = [y; 0]
 *   	Z = scale * [X; sqrt(lambda2) * D] ***/
typedef struct s_linregmodel	linregmodel;

struct s_linregmodel {
	bool		has_copy_y;	// has copy of y
	bool		has_copy_x;	// has copy of x

	mm_dense	*y;		// dense general: observed data vector y (must be dense)
	mm_real	*x;		// sparse/dense symmetric/general: matrix of predictors X
	mm_real	*d;		// sparse/dense symmetric/general: linear operator of penalty D

	double		lambda2;	// weight for penalty term

	mm_dense	*c;				// = x' * y: correlation (constant) vector
	double		log10camax;	// log10 ( amax(c) )

	bool		ycentered;		// y is centered?
	bool		xcentered;		// x is centered?
	bool		xnormalized;	// x is normalized?

	/* sum y. If y is centered, sy = NULL */
	double		*sy;

	/* sum X(:, j). If X is centered, sx = NULL. */
	double		*sx;

	/* xtx = diag(X' * X). If X is normalized, xtx = NULL. */
	double		*xtx;

	/* dtd = diag(D' * D), D = lreg->pen->d */
	double		*dtd;

};

/* linregmodel.c */
linregmodel	*linregmodel_new (mm_dense *y, mm_real *x, const double lambda2, const mm_real *d, PreProc proc);
void			linregmodel_free (linregmodel *l);

#ifdef __cplusplus
}
#endif

#endif /* LINREGMODEL_H_ */
