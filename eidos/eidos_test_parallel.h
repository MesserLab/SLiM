R"V0G0N(


// This file is actually Eidos code!  It is run by _RunUserDefinedFunctionTests() in eidos_test.cpp.
// The purpose of it is to test Eidos functions when run in parallel against the same functions run
// single-threaded, to ensure that parallelization preserves results (to the extent possible).  The
// reason to make this a separate file is mostly because otherwise Xcode's indenting algorithm gets
// very confused.  Note this whole thing is one big C++ string literal.

// Note that this test file gets subdivided and run in chunks; this improves error reporting.  See
// _RunUserDefinedFunctionTests() in eidos_test_functions_other.cpp.

// ***********************************************************************************************

// test that the number of threads is working proerly

if (parallelGetNumThreads() <= 1) stop('running single-threaded in eidos_test_parallel');

parallelSetNumThreads(1);

if (parallelGetNumThreads() != 1) stop('failed to switch to single-threaded in eidos_test_parallel');

// ***********************************************************************************************

// test that the number of threads does not persist across tests

if (parallelGetNumThreads() <= 1) stop('number of threads did not reset in eidos_test_parallel');

// ***********************************************************************************************

// (integer$)sum(logical x)										// EIDOS_OMPMIN_SUM_LOGICAL

x = asLogical(rdunif(1000000, 0, 1));
yN = sum(x);
parallelSetNumThreads(1);
y1 = sum(x);
if (!identical(y1, yN)) stop('parallel sum(logical x) failed test');

// ***********************************************************************************************

// (integer$)sum(integer x)										// EIDOS_OMPMIN_SUM_INTEGER

x = rdunif(1000000, -100, 100);
yN = sum(x);
parallelSetNumThreads(1);
y1 = sum(x);
if (!identical(y1, yN)) stop('parallel sum(integer x) failed test');

// ***********************************************************************************************

// (integer$)sum(float x)										// EIDOS_OMPMIN_SUM_FLOAT

x = runif(1000000, -100, 100);
yN = sum(x);
parallelSetNumThreads(1);
y1 = sum(x);
if (abs(y1 - yN) > 1e-10) stop('parallel sum(float x) failed test');

// ***********************************************************************************************

// (integer)sort(logical x)

x = asLogical(rdunif(1000000, 0, 1));
yN = sort(x);
zN = sort(x, ascending=F);
parallelSetNumThreads(1);
y1 = sort(x);
z1 = sort(x, ascending=F);
if (!identical(y1, yN) | !identical(z1, zN)) stop('parallel sort(logical x) failed test');

// ***********************************************************************************************

// (integer)sort(integer x)

x = rdunif(1000000, -100, 100);
yN = sort(x);
zN = sort(x, ascending=F);
parallelSetNumThreads(1);
y1 = sort(x);
z1 = sort(x, ascending=F);
if (!identical(y1, yN) | !identical(z1, zN)) stop('parallel sort(integer x) failed test');

// ***********************************************************************************************

// (integer)sort(float x)

x = runif(1000000, -100, 100);
yN = sort(x);
zN = sort(x, ascending=F);
parallelSetNumThreads(1);
y1 = sort(x);
z1 = sort(x, ascending=F);
if (!identical(y1, yN) | !identical(z1, zN)) stop('parallel sort(float x) failed test');

// ***********************************************************************************************

//	(float)dnorm(float x, numeric$ mean, numeric$ sd)			// EIDOS_OMPMIN_DNORM_1

x = runif(1000000, -100, 100);
aN = dnorm(x, 12.5, 10.3);
parallelSetNumThreads(1);
a1 = dnorm(x, 12.5, 10.3);
if (!identical(a1, aN)) stop('parallel dnorm() failed test');

// ***********************************************************************************************

//	(float)dnorm(float x, numeric mean, numeric sd)				// EIDOS_OMPMIN_DNORM_2

x = runif(1000000, -100, 100);
mu = runif(1000000, -100, 100);
sigma = runif(1000000, 1.0, 100.0);
bN = dnorm(x, mu, sigma);
parallelSetNumThreads(1);
b1 = dnorm(x, mu, sigma);
if (!identical(b1, bN)) stop('parallel dnorm() failed test');

// ***********************************************************************************************

/*

The above tests can look for identical results, because the functions they are testing
are deterministic (modulo floating point roundoff effects and such).  Below are tests
of functions whose results are stochastic; we test them for correctness using their mean
and variance, and these tests can occasionally fail.  They are calibrated to fail less
than 1 in 1000 runs, with the following (example) R code:

mean_diffs <- rep(0, 1000);
sd_diffs <- rep(0, 1000);

for (i in 1:1000)
{
	x1 = rnorm(1000000, 12.5, 0.1);
	x2 = rnorm(1000000, 12.5, 0.1);
	mean_diffs[i] <- abs(mean(x1)-mean(x2));
	sd_diffs[i] <- abs(sd(x1)-sd(x2));
}

max(mean_diffs)
max(sd_diffs)

*/

// ***********************************************************************************************

//	(integer)rbinom(integer$ n, integer$ size = 1, float$ prob = 0.5)	// EIDOS_OMPMIN_RBINOM_1

aN = rbinom(1000000, 1, 0.5);
parallelSetNumThreads(1);
a1 = rbinom(1000000, 1, 0.5);
if (abs(mean(aN)-mean(a1)) > 0.003) stop('parallel rbinom() failed mean test');
if (abs(sd(aN)-sd(a1)) > 0.000003) stop('parallel rbinom() failed sd test');

// ***********************************************************************************************

//	(integer)rbinom(integer$ n, integer$ size, float$ prob)				// EIDOS_OMPMIN_RBINOM_2

aN = rbinom(1000000, 3, 0.1);
parallelSetNumThreads(1);
a1 = rbinom(1000000, 3, 0.1);
if (abs(mean(aN)-mean(a1)) > 0.003) stop('parallel rbinom() failed mean test');
if (abs(sd(aN)-sd(a1)) > 0.003) stop('parallel rbinom() failed sd test');

// ***********************************************************************************************

//	(integer)rbinom(integer$ n, integer size, float prob)				// EIDOS_OMPMIN_RBINOM_3

size = rep(3, 1000000);
prob = rep(0.1, 1000000);
aN = rbinom(1000000, size, prob);
parallelSetNumThreads(1);
a1 = rbinom(1000000, size, prob);
if (abs(mean(aN)-mean(a1)) > 0.003) stop('parallel rbinom() failed mean test');
if (abs(sd(aN)-sd(a1)) > 0.003) stop('parallel rbinom() failed sd test');

// ***********************************************************************************************

//	(integer)rdunif(integer$ n, integer$ min = 0, integer$ max = 1)		// EIDOS_OMPMIN_RDUNIF_1

aN = rdunif(1000000, 3, 4);
parallelSetNumThreads(1);
a1 = rdunif(1000000, 3, 4);
if (abs(mean(aN)-mean(a1)) > 0.003) stop('parallel rdunif() failed mean test');
if (abs(sd(aN)-sd(a1)) > 0.000003) stop('parallel rdunif() failed sd test');

// ***********************************************************************************************

//	(integer)rdunif(integer$ n, integer$ min, integer$ max)				// EIDOS_OMPMIN_RDUNIF_2

aN = rdunif(1000000, 3, 17);
parallelSetNumThreads(1);
a1 = rdunif(1000000, 3, 17);
if (abs(mean(aN)-mean(a1)) > 0.025) stop('parallel rdunif() failed mean test');
if (abs(sd(aN)-sd(a1)) > 0.011) stop('parallel rdunif() failed sd test');


// ***********************************************************************************************

//	(integer)rdunif(integer$ n, integer min, integer max)				// EIDOS_OMPMIN_RDUNIF_3

min = rep(3, 1000000);
max = rep(17, 1000000);
aN = rdunif(1000000, min, max);
parallelSetNumThreads(1);
a1 = rdunif(1000000, min, max);
if (abs(mean(aN)-mean(a1)) > 0.025) stop('parallel rdunif() failed mean test');
if (abs(sd(aN)-sd(a1)) > 0.011) stop('parallel rdunif() failed sd test');

// ***********************************************************************************************

//	(float)rexp(integer$ n, numeric$ mu)								// EIDOS_OMPMIN_REXP_1
aN = rexp(1000000, 0.2);
parallelSetNumThreads(1);
a1 = rexp(1000000, 0.2);
if (abs(mean(aN)-mean(a1)) > 0.0015) stop('parallel rexp() failed mean test');
if (abs(sd(aN)-sd(a1)) > 0.0015) stop('parallel rexp() failed sd test');

// ***********************************************************************************************

//	(float)rexp(integer$ n, numeric mu)									// EIDOS_OMPMIN_REXP_2

mu = rep(0.2, 1000000);
aN = rexp(1000000, 0.2);
parallelSetNumThreads(1);
a1 = rexp(1000000, 0.2);
if (abs(mean(aN)-mean(a1)) > 0.0015) stop('parallel rexp() failed mean test');
if (abs(sd(aN)-sd(a1)) > 0.0015) stop('parallel rexp() failed sd test');

// ***********************************************************************************************

//	(float)rnorm(integer$ n, numeric$ mean, numeric$ sd)				// EIDOS_OMPMIN_RNORM_1

aN = rnorm(1000000, 12.5, 0.1);
parallelSetNumThreads(1);
a1 = rnorm(1000000, 12.5, 0.1);
if (abs(mean(aN)-mean(a1)) > 0.0005) stop('parallel rnorm() failed mean test');
if (abs(sd(aN)-sd(a1)) > 0.0005) stop('parallel rnorm() failed sd test');

// ***********************************************************************************************

//	(float)rnorm(integer$ n, numeric mean, numeric$ sd)					// EIDOS_OMPMIN_RNORM_2

mu = rep(12.5, 1000000);
aN = rnorm(1000000, mu, 0.1);
parallelSetNumThreads(1);
a1 = rnorm(1000000, mu, 0.1);
if (abs(mean(aN)-mean(a1)) > 0.0005) stop('parallel rnorm() failed mean test');
if (abs(sd(aN)-sd(a1)) > 0.0005) stop('parallel rnorm() failed sd test');

// ***********************************************************************************************

//	(float)rnorm(integer$ n, numeric mean, numeric sd)					// EIDOS_OMPMIN_RNORM_3

mu = rep(12.5, 1000000);
sigma = rep(0.1, 1000000);
aN = rnorm(1000000, mu, sigma);
parallelSetNumThreads(1);
a1 = rnorm(1000000, mu, sigma);
if (abs(mean(aN)-mean(a1)) > 0.0005) stop('parallel rnorm() failed mean test');
if (abs(sd(aN)-sd(a1)) > 0.0005) stop('parallel rnorm() failed sd test');

// ***********************************************************************************************

//	(integer)rpois(integer$ n, numeric$ lambda)							// EIDOS_OMPMIN_RPOIS_1

aN = rpois(1000000, 15);
parallelSetNumThreads(1);
a1 = rpois(1000000, 15);
if (abs(mean(aN)-mean(a1)) > 0.02) stop('parallel rpois() failed mean test');
if (abs(sd(aN)-sd(a1)) > 0.02) stop('parallel rpois() failed sd test');

// ***********************************************************************************************

//	(integer)rpois(integer$ n, numeric lambda)							// EIDOS_OMPMIN_RPOIS_2

lambda = rep(15, 1000000);
aN = rpois(1000000, lambda);
parallelSetNumThreads(1);
a1 = rpois(1000000, lambda);
if (abs(mean(aN)-mean(a1)) > 0.02) stop('parallel rpois() failed mean test');
if (abs(sd(aN)-sd(a1)) > 0.02) stop('parallel rpois() failed sd test');

// ***********************************************************************************************

//	(float)runif(integer$ n, numeric$ min = 0], numeric$ max = 1)		// EIDOS_OMPMIN_RUNIF_1

aN = runif(1000000, 0, 1);
parallelSetNumThreads(1);
a1 = runif(1000000, 0, 1);
if (abs(mean(aN)-mean(a1)) > 0.0015) stop('parallel rpois() failed mean test');
if (abs(sd(aN)-sd(a1)) > 0.0006) stop('parallel rpois() failed sd test');

// ***********************************************************************************************

//	(float)runif(integer$ n, numeric$ min, numeric$ max)				// EIDOS_OMPMIN_RUNIF_2

aN = runif(1000000, 3, 17);
parallelSetNumThreads(1);
a1 = runif(1000000, 3, 17);
if (abs(mean(aN)-mean(a1)) > 0.02) stop('parallel rpois() failed mean test');
if (abs(sd(aN)-sd(a1)) > 0.01) stop('parallel rpois() failed sd test');

// ***********************************************************************************************

//	(float)runif(integer$ n, numeric min, numeric max)					// EIDOS_OMPMIN_RUNIF_3

min = rep(3.0, 1000000);
max = rep(17.0, 1000000);
aN = runif(1000000, 3, 17);
parallelSetNumThreads(1);
a1 = runif(1000000, 3, 17);
if (abs(mean(aN)-mean(a1)) > 0.02) stop('parallel rpois() failed mean test');
if (abs(sd(aN)-sd(a1)) > 0.01) stop('parallel rpois() failed sd test');

)V0G0N"








































