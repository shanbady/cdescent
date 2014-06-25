/*
 * example_cdescent.c
 *
 *  Created on: 2014/05/27
 *      Author: utsugi
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cdescent.h>

static void
output_solutionpath_cdescent (int iter, const cdescent *cd)
{
	int			i;
	char		fn[80];
	FILE		*fp;

	for (i = 0; i < cd->lreg->x->n; i++) {

		sprintf (fn, "beta%03d.res", i);

		if (iter == 0) fp = fopen (fn, "w");
		else fp = fopen (fn, "aw");
		if (fp == NULL) continue;

		fprintf (fp, "%d\t%.4e\t%.4e\n", iter, cd->nrm1, cd->beta->data[i]);
		fclose (fp);
	}
	return;
}

void
example_cdescent_pathwise (const linreg *lreg, double logtmin, double dlogt, double logtmax, double tol, int maxiter)
{
	int			iter = 0;
	double		logt;
	cdescent	*cd;

	/* warm start */
	cd = cdescent_new (lreg, tol);
	logt = (cd->logcamax <= logtmax) ? cd->logcamax : logtmax;

	while (logtmin <= logt) {

		fprintf (stdout, "t = %.4e, intercept = %.4e\n", cd->lambda1, cd->b);
		if (!cdescent_cyclic (cd, maxiter)) break;
		output_solutionpath_cdescent (iter, cd);

		logt -= dlogt;
		cdescent_set_log10_lambda1 (cd, logt);

		iter++;
	}

	return;
}
