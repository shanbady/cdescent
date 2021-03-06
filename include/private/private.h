/*
 * private.h
 *
 *  Created on: 2014/05/14
 *      Author: utsugi
 */

#ifndef PRIVATE_H
#define PRIVATE_H

#include <float.h>

/* Private macros, constants and headers
 * which are only used internally */

/* blas */
#ifdef HAVE_BLAS_H
#include <blas.h>
#else
// Level1
extern double	dasum_  (const int *n, const double *x, const int *incx);
extern void	daxpy_  (const int *n, const double *alpha, const double *x, const int *incx, double *y, const int *incy);
extern void	dcopy_  (const int *n, const double *x, const int *incx, double *y, const int *incy);
extern double	ddot_   (const int *n, const double *x, const int *incx, const double *y, const int *incy);
extern double	dnrm2_  (const int *n, const double *x, const int *incx);
extern void	dscal_  (const int *n, const double *alpha, double *x, const int *incx);
extern int		idamax_ (const int *n, const double *x, const int *incx);
// Level2
extern void	dgemv_ (const char *trans, const int *m, const int *n, const double *alpha, const double *a, const int *lda,
		const double *x, const int *incx, const double *beta, double *y, const int *incy);
extern void	dsymv_ (const char *uplo, const int *n, const double *alpha, const double *a, const int *lda,
		const double *x, const int *incx, const double *beta, double *y, const int *incy);
// Level3
extern void	dgemm_ (const char *transa, const char *transb, const int *m, const int *n, const int *k,
		const double *alpha, const double *a, const int *lda, const double *b, const int *ldb,
		const double *beta, double *c, const int *ldc);
#endif

/* following constants are set in private.c */
extern const int		ione;	//  1
extern const double	dzero;	//  0.
extern const double	done;	//  1.
extern const double	dmone;	// -1.

/* positive infinity  */
#define CDESCENT_POSINF	((+1.)/(+0.))

/* DBL_EPSILOM */
#ifndef DBL_EPSILON
#define DBL_EPSILON		2.2204460492503131e-16
#endif

/* SQRT_DBL_EPSILOM */
#ifndef SQRT_DBL_EPSILON
#define SQRT_DBL_EPSILON	1.4901161193847656e-08
#endif

/* print error message and terminate program */
void	error_and_exit (const char * function_name, const char *error_msg, const char *file, const int line);
/* print warning message */
void	printf_warning (const char * function_name, const char *error_msg, const char *file, const int line);

#endif /* PRIVATE_H */
