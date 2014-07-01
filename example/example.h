/*
 * example.h
 *
 *  Created on: 2014/03/17
 *      Author: utsugi
 */

#ifndef EXAMPLE_H_
#define EXAMPLE_H_

/* example.c */
void	read_data (char *fn, int skip_header, int *n, int *p, double **y, double **x);

/* example_cdescent.c */
void	example_cdescent_pathwise (const linregmodel *lreg, double start, double dt, double stop, double tol, int maxiter, bool parallel);

#endif /* EXAMPLE_H_ */
