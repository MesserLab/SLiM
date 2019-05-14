R"V0G0N(


// This file is actually Eidos code!  It is run by _RunUserDefinedFunctionTests() in eidos_test.cpp.
// The purpose of it is to test the correctness of the built-in Eidos functions by mimicking them
// with equivalent Eidos user-defined functions, and then compare the two using random data.  The
// reason to make this a separate file is mostly because otherwise Xcode's indenting algorithm gets
// very confused.  Note this whole thing is one big C++ string literal.

// Note that the Eidos versions of the built-in functions here are not necessarily as general or
// robust as the built-in functions.  The user-defined functions here are intended only for testing
// the cases that are exercised by the test code here, and may fail in other cases.  The goal here
// is not so much checking for robustness in all cases, with all types of arguments, etc. (which is
// tested in eidos_test.cpp), as testing for algorithmic correctness using repeated tests with
// random data.  These tests are thus complementary to those in eidos_test.cpp.

// Note that this test file gets subdivided and run in chunks; this improves error reporting.  See
// _RunUserDefinedFunctionTests() in eidos_test.cpp.

// ***********************************************************************************************

// (numeric)abs(numeric x)
function (numeric)abs_func(numeric x)
{
	return sapply(x, 'if (applyValue < 0) -applyValue; else applyValue;');
}

x = sample(-1000:1000, 10000, T);	// integer
xbuiltin = abs(x);
xuserdef = abs_func(x);
if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of abs(i)');

x = rnorm(10000);	// float
xbuiltin = abs(x);
xuserdef = abs_func(x);
if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of abs(f)');

// ***********************************************************************************************

// (logical$)all(logical x, ...)
function (l$)all_func(l x)
{
	return sum(x) == size(x);
}

for (iter in 1:10000)
{
	x = asLogical(rbinom(10, 1, 0.5));
	xbuiltin = all(x);
	xuserdef = all_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of all()');
}

for (iter in 1:100)
{
	x1 = asLogical(rbinom(rdunif(1, 0, 3), 1, 0.5));
	x2 = asLogical(rbinom(rdunif(1, 0, 3), 1, 0.5));
	x3 = asLogical(rbinom(rdunif(1, 0, 3), 1, 0.5));
	x = c(x1, x2, x3);
	xbuiltin = all(x1, x2, x3);
	xuserdef = all_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of all(l, l, l)');
}

// ***********************************************************************************************

// (logical$)any(logical x, ...)
function (l$)any_func(l x)
{
	return sum(x) > 0;
}

for (iter in 1:10000)
{
	x = asLogical(rbinom(10, 1, 0.5));
	xbuiltin = any(x);
	xuserdef = any_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of any()');
}

for (iter in 1:100)
{
	x1 = asLogical(rbinom(rdunif(1, 0, 3), 1, 0.5));
	x2 = asLogical(rbinom(rdunif(1, 0, 3), 1, 0.5));
	x3 = asLogical(rbinom(rdunif(1, 0, 3), 1, 0.5));
	x = c(x1, x2, x3);
	xbuiltin = any(x1, x2, x3);
	xuserdef = any_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of any(l, l, l)');
}

// ***********************************************************************************************

// (numeric)cumProduct(numeric x)
function (numeric)cumProduct_func(numeric x)
{
	return sapply(seqAlong(x), 'product(x[0:applyValue]);');
}

for (iter in 1:100)
{
	x = sample(-100:100, 5, T);	// integer
	xbuiltin = cumProduct(x);
	xuserdef = cumProduct_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of cumProduct(i)');
}

for (iter in 1:100)
{
	x = rnorm(10);		// float
	xbuiltin = cumProduct(x);
	xuserdef = cumProduct_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of cumProduct(f)');
}

// ***********************************************************************************************

// (numeric)cumSum(numeric x)
function (numeric)cumSum_func(numeric x)
{
	return sapply(seqAlong(x), 'sum(x[0:applyValue]);');
}

for (iter in 1:100)
{
	x = sample(-1000:1000, 10, T);	// integer
	xbuiltin = cumSum(x);
	xuserdef = cumSum_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of cumSum(i)');
}

for (iter in 1:100)
{
	x = rnorm(10);		// float
	xbuiltin = cumSum(x);
	xuserdef = cumSum_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of cumSum(f)');
}

// ***********************************************************************************************

// (float)exp(numeric x)
function (f)exp_func(numeric x)
{
	return E ^ x;	// this uses pow(), which may produce slightly different results than exp()
}

x = sample(-100:100, 10000, T);	// integer
xbuiltin = exp(x);
xuserdef = exp_func(x);
if (any(abs(xbuiltin / xuserdef - 1.0) > 1e-10)) stop('Mismatch in test of exp(i)');

x = rnorm(10000);	// float
xbuiltin = exp(x);
xuserdef = exp_func(x);
if (any(abs(xbuiltin / xuserdef - 1.0) > 1e-10)) stop('Mismatch in test of exp(f)');

// ***********************************************************************************************

// (*)ifelse(logical test, * trueValues, * falseValues)
function (*)ifelse_func(l test, * tvals, * fvals)
{
	return sapply(seqAlong(test), "test[applyValue] ? tvals[applyValue] else fvals[applyValue];");
}

for (iter in 1:10000)
{
	x = asLogical(rbinom(10, 1, 0.5));
	tvals = rnorm(10);
	fvals = rnorm(10);
	xbuiltin = ifelse(x, tvals, fvals);
	xuserdef = ifelse_func(x, tvals, fvals);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of ifelse()');
}

// ***********************************************************************************************

// (+$)max(+ x, ...)
function (+$)max_func(+ x)
{
	if (size(x) == 0)
		return NULL;
	return sort(x)[size(x) - 1];
}

for (iter in 1:100)
{
	x = sample(c(F,T), 10, T);	// logical
	xbuiltin = max(x);
	xuserdef = max_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of max(l)');
}

for (iter in 1:100)
{
	x1 = sample(c(F,T), rdunif(1, 0, 3), T);
	x2 = sample(c(F,T), rdunif(1, 0, 3), T);
	x3 = sample(c(F,T), rdunif(1, 0, 3), T);
	x = c(x1, x2, x3);
	xbuiltin = max(x1, x2, x3);
	xuserdef = max_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of max(l, l, l)');
}

for (iter in 1:100)
{
	x = sample(-100:100, 10, T);	// integer
	xbuiltin = max(x);
	xuserdef = max_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of max(i)');
}

for (iter in 1:100)
{
	x1 = sample(-10:10, rdunif(1, 0, 3), T);
	x2 = sample(-10:10, rdunif(1, 0, 3), T);
	x3 = sample(-10:10, rdunif(1, 0, 3), T);
	x = c(x1, x2, x3);
	xbuiltin = max(x1, x2, x3);
	xuserdef = max_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of max(i, i, i)');
}

for (iter in 1:100)
{
	x = rnorm(10);		// float
	xbuiltin = max(x);
	xuserdef = max_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of max(f)');
}

for (iter in 1:100)
{
	x1 = sample(-10.0:10.0, rdunif(1, 0, 3), T);
	x2 = sample(-10.0:10.0, rdunif(1, 0, 3), T);
	x3 = sample(-10.0:10.0, rdunif(1, 0, 3), T);
	x = c(x1, x2, x3);
	xbuiltin = max(x1, x2, x3);
	xuserdef = max_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of max(f, f, f)');
}

// ***********************************************************************************************

// (float$)mean(lif x)
function (f$)mean_func(lif x)
{
	return sum(x) / size(x);
}

for (iter in 1:100)
{
	x = sample(c(T, F), 10, T);	// logical
	xbuiltin = mean(x);
	xuserdef = mean_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of mean(l)');
}

for (iter in 1:100)
{
	x = sample(-100:100, 10, T);	// integer
	xbuiltin = mean(x);
	xuserdef = mean_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of mean(i)');
}

for (iter in 1:100)
{
	x = rnorm(10);		// float
	xbuiltin = mean(x);
	xuserdef = mean_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of mean(f)');
}

// ***********************************************************************************************

// (+$)min(+ x, ...)
function (+$)min_func(+ x)
{
	if (size(x) == 0)
		return NULL;
	return sort(x)[0];
}

for (iter in 1:100)
{
	x = sample(c(F,T), 10, T);	// logical
	xbuiltin = min(x);
	xuserdef = min_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of min(l)');
}

for (iter in 1:100)
{
	x1 = sample(c(F,T), rdunif(1, 0, 3), T);
	x2 = sample(c(F,T), rdunif(1, 0, 3), T);
	x3 = sample(c(F,T), rdunif(1, 0, 3), T);
	x = c(x1, x2, x3);
	xbuiltin = min(x1, x2, x3);
	xuserdef = min_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of min(l, l, l)');
}

for (iter in 1:100)
{
	x = sample(-100:100, 10, T);	// integer
	xbuiltin = min(x);
	xuserdef = min_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of min(i)');
}

for (iter in 1:100)
{
	x1 = sample(-10:10, rdunif(1, 0, 3), T);
	x2 = sample(-10:10, rdunif(1, 0, 3), T);
	x3 = sample(-10:10, rdunif(1, 0, 3), T);
	x = c(x1, x2, x3);
	xbuiltin = min(x1, x2, x3);
	xuserdef = min_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of min(i, i, i)');
}

for (iter in 1:100)
{
	x = rnorm(10);		// float
	xbuiltin = min(x);
	xuserdef = min_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of min(f)');
}

for (iter in 1:100)
{
	x1 = sample(-10.0:10.0, rdunif(1, 0, 3), T);
	x2 = sample(-10.0:10.0, rdunif(1, 0, 3), T);
	x3 = sample(-10.0:10.0, rdunif(1, 0, 3), T);
	x = c(x1, x2, x3);
	xbuiltin = min(x1, x2, x3);
	xuserdef = min_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of min(f, f, f)');
}

// ***********************************************************************************************

// (+)pmax(+ x, + y)
function (+)pmax_func(+x, +y)
{
	return ifelse(x > y, x, y);
}

x = sample(c(F,T), 10000, T);	// logical
y = sample(c(F,T), 10000, T);
xbuiltin = pmax(x, y);
xuserdef = pmax_func(x, y);
if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of pmax(l)');

x = sample(-100:100, 10000, T);	// integer
y = sample(-100:100, 10000, T);
xbuiltin = pmax(x, y);
xuserdef = pmax_func(x, y);
if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of pmax(i)');

x = rnorm(10000);	// float
y = rnorm(10000);
xbuiltin = pmax(x, y);
xuserdef = pmax_func(x, y);
if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of pmax(f)');

// ***********************************************************************************************

// (+)pmin(+ x, + y)
function (+)pmin_func(+x, +y)
{
	return ifelse(x < y, x, y);
}

x = sample(c(F,T), 10000, T);	// logical
y = sample(c(F,T), 10000, T);
xbuiltin = pmin(x, y);
xuserdef = pmin_func(x, y);
if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of pmin(l)');

x = sample(-100:100, 10000, T);	// integer
y = sample(-100:100, 10000, T);
xbuiltin = pmin(x, y);
xuserdef = pmin_func(x, y);
if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of pmin(i)');

x = rnorm(10000);	// float
y = rnorm(10000);
xbuiltin = pmin(x, y);
xuserdef = pmin_func(x, y);
if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of pmin(f)');

// ***********************************************************************************************

// (numeric$)product(numeric x)
function (numeric$)product_func(numeric x)
{
	p = 1;
	
	sapply(x, 'p = p * applyValue; NULL;');
	
	return p;
}

for (iter in 1:100)
{
	x = sample(-100:100, 5, T);	// integer
	xbuiltin = product(x);
	xuserdef = product_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of product(i)');
}

for (iter in 1:100)
{
	x = rnorm(10);		// float
	xbuiltin = product(x);
	xuserdef = product_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of product(f)');
}

// ***********************************************************************************************

// (numeric)range(numeric x, ...)
function (numeric)range_func(numeric x)
{
	if (size(x) == 0)
		return NULL;
	sx = sort(x);
	return c(sx[0], sx[size(x) - 1]);
}

for (iter in 1:100)
{
	x = sample(-100:100, 10, T);	// integer
	xbuiltin = range(x);
	xuserdef = range_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of range(i)');
}

for (iter in 1:100)
{
	x1 = sample(-10:10, rdunif(1, 0, 3), T);
	x2 = sample(-10:10, rdunif(1, 0, 3), T);
	x3 = sample(-10:10, rdunif(1, 0, 3), T);
	x = c(x1, x2, x3);
	xbuiltin = range(x1, x2, x3);
	xuserdef = range_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of range(i, i, i)');
}

for (iter in 1:100)
{
	x = rnorm(10);		// float
	xbuiltin = range(x);
	xuserdef = range_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of range(f)');
}

for (iter in 1:100)
{
	x1 = sample(-10.0:10.0, rdunif(1, 0, 3), T);
	x2 = sample(-10.0:10.0, rdunif(1, 0, 3), T);
	x3 = sample(-10.0:10.0, rdunif(1, 0, 3), T);
	x = c(x1, x2, x3);
	xbuiltin = range(x1, x2, x3);
	xuserdef = range_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of range(f, f, f)');
}

// ***********************************************************************************************

// (*)rep(* x, integer$ count)
function (*)rep_func(* x, i$ count)
{
	r = NULL;
	
	for (i in 1:count)
		r = c(r, x);
	
	return r;
}

for (iter in 1:100)
{
	x = sample(c(F,T), 10, T);	// logical
	xbuiltin = rep(x, 5);
	xuserdef = rep_func(x, 5);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of rep(l)');
}

for (iter in 1:100)
{
	x = sample(-100:100, 10, T);	// integer
	xbuiltin = rep(x, 5);
	xuserdef = rep_func(x, 5);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of rep(i)');
}

for (iter in 1:100)
{
	x = rnorm(10);		// float
	xbuiltin = rep(x, 5);
	xuserdef = rep_func(x, 5);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of rep(f)');
}

// ***********************************************************************************************

// (*)repEach(* x, integer count)
function (*)repEach_func(* x, i$ count)
{
	r = NULL;
	
	for (i in seqAlong(x))
		r = c(r, rep(x[i], count));
	
	return r;
}

for (iter in 1:100)
{
	x = sample(c(F,T), 10, T);	// logical
	xbuiltin = repEach(x, 5);
	xuserdef = repEach_func(x, 5);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of repEach(l)');
}

for (iter in 1:100)
{
	x = sample(-100:100, 10, T);	// integer
	xbuiltin = repEach(x, 5);
	xuserdef = repEach_func(x, 5);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of repEach(i)');
}

for (iter in 1:100)
{
	x = rnorm(10);		// float
	xbuiltin = repEach(x, 5);
	xuserdef = repEach_func(x, 5);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of repEach(f)');
}

// ***********************************************************************************************

// (*)rev(* x)
function (*)rev_func(* x)
{
	return x[(size(x) - 1):0];
}

for (iter in 1:100)
{
	x = sample(c(F,T), 10, T);	// logical
	xbuiltin = rev(x);
	xuserdef = rev_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of rev(l)');
}

for (iter in 1:100)
{
	x = sample(-100:100, 10, T);	// integer
	xbuiltin = rev(x);
	xuserdef = rev_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of rev(i)');
}

for (iter in 1:100)
{
	x = rnorm(10);		// float
	xbuiltin = rev(x);
	xuserdef = rev_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of rev(f)');
}

// ***********************************************************************************************

// (float$)sd(numeric x)
function (f$)sd_func(numeric x)
{
	ss = sum((x - mean(x)) ^ 2);
	return sqrt(ss / (size(x) - 1));
}

for (iter in 1:100)
{
	x = sample(-100:100, 10, T);	// integer
	xbuiltin = sd(x);
	xuserdef = sd_func(x);
	if (abs(xbuiltin - xuserdef) > 1e-10) stop('Mismatch in test of sd(i)');
}

for (iter in 1:100)
{
	x = rnorm(10);		// float
	xbuiltin = sd(x);
	xuserdef = sd_func(x);
	if (abs(xbuiltin - xuserdef) > 1e-10) stop('Mismatch in test of sd(f)');
}

// ***********************************************************************************************

// (*)setDifference(* x, * y)
function (*)setDifference_func(* x, * y)
{
	return setSymmetricDifference(x, setIntersection(x, y));
}

for (iter in 1:100)
{
	x = sample(c(F,T), 10, T);	// logical
	y = sample(c(F,T), 10, T);
	xbuiltin = setDifference(x, y);
	xuserdef = setDifference_func(x, y);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of setDifference(l)');
}

for (iter in 1:100)
{
	x = sample(-100:100, 10, F);	// integer
	y = sample(-100:100, 10, F);
	xbuiltin = setDifference(x, y);
	xuserdef = setDifference_func(x, y);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of setDifference(i)');
}

for (iter in 1:100)
{
	x = sample(-100.0:100.0, 10, F);	// float
	y = sample(-100.0:100.0, 10, F);
	xbuiltin = setDifference(x, y);
	xuserdef = setDifference_func(x, y);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of setDifference(f)');
}

// ***********************************************************************************************

// (*)setIntersection(* x, * y)
function (*)setIntersection_func(* x, * y)
{
	return setDifference(setUnion(x, y), setSymmetricDifference(x, y));
}

for (iter in 1:100)
{
	x = sample(c(F,T), 10, T);	// logical
	y = sample(c(F,T), 10, T);
	xbuiltin = setIntersection(x, y);
	xuserdef = setIntersection_func(x, y);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of setIntersection(l)');
}

for (iter in 1:100)
{
	x = sample(-100:100, 10, F);	// integer
	y = sample(-100:100, 10, F);
	xbuiltin = setIntersection(x, y);
	xuserdef = setIntersection_func(x, y);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of setIntersection(i)');
}

for (iter in 1:100)
{
	x = sample(-100.0:100.0, 10, F);	// float
	y = sample(-100.0:100.0, 10, F);
	xbuiltin = setIntersection(x, y);
	xuserdef = setIntersection_func(x, y);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of setIntersection(f)');
}

// ***********************************************************************************************

// (*)setSymmetricDifference(* x, * y)
function (*)setSymmetricDifference_func(* x, * y)
{
	return setDifference(setUnion(x, y), setIntersection(x, y));
}

for (iter in 1:100)
{
	x = sample(c(F,T), 10, T);	// logical
	y = sample(c(F,T), 10, T);
	xbuiltin = setSymmetricDifference(x, y);
	xuserdef = setSymmetricDifference_func(x, y);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of setSymmetricDifference(l)');
}

for (iter in 1:100)
{
	x = sample(-100:100, 10, F);	// integer
	y = sample(-100:100, 10, F);
	xbuiltin = setSymmetricDifference(x, y);
	xuserdef = setSymmetricDifference_func(x, y);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of setSymmetricDifference(i)');
}

for (iter in 1:100)
{
	x = sample(-100.0:100.0, 10, F);	// float
	y = sample(-100.0:100.0, 10, F);
	xbuiltin = setSymmetricDifference(x, y);
	xuserdef = setSymmetricDifference_func(x, y);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of setSymmetricDifference(f)');
}

// ***********************************************************************************************

// (*)setUnion(* x, * y)
function (*)setUnion_func(* x, * y)
{
	return unique(c(x, y));
}

for (iter in 1:100)
{
	x = sample(c(F,T), 10, T);	// logical
	y = sample(c(F,T), 10, T);
	xbuiltin = sort(setUnion(x, y));
	xuserdef = sort(setUnion_func(x, y));
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of setUnion(l)');
}

for (iter in 1:100)
{
	x = sample(-100:100, 10, F);	// integer
	y = sample(-100:100, 10, F);
	xbuiltin = setUnion(x, y);
	xuserdef = setUnion_func(x, y);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of setUnion(i)');
}

for (iter in 1:100)
{
	x = sample(-100.0:100.0, 10, F);	// float
	y = sample(-100.0:100.0, 10, F);
	xbuiltin = setUnion(x, y);
	xuserdef = setUnion_func(x, y);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of setUnion(f)');
}

// ***********************************************************************************************

// (float)sqrt(numeric x)
function (f)sqrt_func(numeric x)
{
	return x ^ 0.5;	// this uses pow(), which may produce slightly different results than sqrt()
}

x = sample(1:100, 10000, T);	// integer
xbuiltin = sqrt(x);
xuserdef = sqrt_func(x);
if (any(abs(xbuiltin / xuserdef - 1.0) > 1e-10)) stop('Mismatch in test of sqrt(i)');

x = runif(10000, 0.0001, 100000);	// float
xbuiltin = sqrt(x);
xuserdef = sqrt_func(x);
if (any(abs(xbuiltin / xuserdef - 1.0) > 1e-10)) stop('Mismatch in test of sqrt(f)');

// ***********************************************************************************************

// (numeric$)sum(lif x)
function (numeric$)sum_func(numeric x)
{
	s = 0;
	
	sapply(x, 's = s + applyValue; NULL;');
	
	return s;
}

for (iter in 1:100)
{
	x = sample(c(F,T), 10, T);	// logical
	xbuiltin = sum(x);
	xuserdef = size(which(x));
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of sum(l)');
}

for (iter in 1:100)
{
	x = sample(-100:100, 10, T);	// integer
	xbuiltin = sum(x);
	xuserdef = sum_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of sum(i)');
}

for (iter in 1:100)
{
	x = rnorm(10);		// float
	xbuiltin = sum(x);
	xuserdef = sum_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of sum(f)');
}

// ***********************************************************************************************

// (float$)sumExact(float x)
function (f$)sumExact_func(f x)
{
	s = 0;
	
	sapply(x, 's = s + applyValue; NULL;');	// no attempt to sum exactly, just using tolerance limits below
	
	return s;
}

for (iter in 1:100)
{
	x = rnorm(10);
	xbuiltin = sumExact(x);
	xuserdef = sumExact_func(x);
	if (abs(xbuiltin / xuserdef - 1.0) > 1e-10) stop('Mismatch in test of sumExact(f)');
}

// ***********************************************************************************************

// (*)unique(* x, [logical$ preserveOrder = T])
function (*)unique_func(* x)
{
	s = sort(x);	// this implementation does not preserve order!
	r = s[0];
	p = r;
	for (i in 1:(size(x) - 1))
	{
		v = s[i];
		if (v == p) next;
		r = c(r, v);
		p = v;
	}
	return r;
}

for (iter in 1:100)
{
	x = sample(c(F,T), 5, T);	// logical
	xbuiltin = sort(unique(x, F));
	xuserdef = sort(unique_func(x));
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of unique(l, F)');
}

for (iter in 1:100)
{
	x = sample(c(F,T), 5, T);	// logical
	xbuiltin = sort(unique(x, T));
	xuserdef = sort(unique_func(x));
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of unique(l, T)');
}

for (iter in 1:100)
{
	x = sample(-100:100, 100, T);	// integer
	xbuiltin = unique(x, F);
	xuserdef = unique_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of unique(i, F)');
}

for (iter in 1:100)
{
	x = sample(-100:100, 100, T);	// integer
	xbuiltin = sort(unique(x, T));
	xuserdef = sort(unique_func(x));
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of unique(i, T)');
}

for (iter in 1:100)
{
	x = sample(-100.0:100.0, 100, T);	// float
	xbuiltin = unique(x, F);
	xuserdef = unique_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of unique(f, F)');
}

for (iter in 1:100)
{
	x = sample(-100.0:100.0, 100, T);	// float
	xbuiltin = sort(unique(x, T));
	xuserdef = sort(unique_func(x));
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of unique(f, T)');
}

// ***********************************************************************************************

// (integer)which(logical x)
function (i)which_func(l x)
{
	w = sapply(seqAlong(x), 'x[applyValue] ? applyValue else NULL;');
	
	return isNULL(w) ? integer(0) else w;
}

for (iter in 1:10000)
{
	x = asLogical(rbinom(10, 1, 0.5));
	xbuiltin = which(x);
	xuserdef = which_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of which()');
}

// ***********************************************************************************************

// (integer$)whichMax(+ x)
function (integer$)whichMax_func(+ x)
{
	return min(which(x == max(x)));
}

for (iter in 1:100)
{
	x = sample(c(F,T), 10, T);	// logical
	xbuiltin = whichMax(x);
	xuserdef = whichMax_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of whichMax(l)');
}

for (iter in 1:100)
{
	x = sample(-100:100, 10, T);	// integer
	xbuiltin = whichMax(x);
	xuserdef = whichMax_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of whichMax(i)');
}

for (iter in 1:100)
{
	x = rnorm(10);		// float
	xbuiltin = whichMax(x);
	xuserdef = whichMax_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of whichMax(f)');
}

// ***********************************************************************************************

// (integer$)whichMin(+ x)
function (integer$)whichMin_func(+ x)
{
	return min(which(x == min(x)));
}

for (iter in 1:100)
{
	x = sample(c(F,T), 10, T);	// logical
	xbuiltin = whichMin(x);
	xuserdef = whichMin_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of whichMin(l)');
}

for (iter in 1:100)
{
	x = sample(-100:100, 10, T);	// integer
	xbuiltin = whichMin(x);
	xuserdef = whichMin_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of whichMin(i)');
}

for (iter in 1:100)
{
	x = rnorm(10);		// float
	xbuiltin = whichMin(x);
	xuserdef = whichMin_func(x);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of whichMin(f)');
}

// ***********************************************************************************************

// (integer)integer(integer$ length, [integer$ fill1 = 0], [integer$ fill2 = 1], [Ni fill2Indices = NULL])
function (integer)integer_func(integer$ length, integer$ fill1, integer$ fill2, i fill2Indices)
{
	x = rep(fill1, length);
	x[fill2Indices] = fill2;
	return x;
}

for (iter in 1:1000)
{
	indices = sample(0:999, rdunif(1, 0, 100));
	xbuiltin = integer(1000, 5, 8, indices);
	xuserdef = integer_func(1000, 5, 8, indices);
	if (!identical(xbuiltin, xuserdef)) stop('Mismatch in test of integer()');
}

// ***********************************************************************************************

// The functions above are the ones that seemed simple and worthwhile to test by replication.
// Since we're in a groove, let's test some other Eidos functions, not by replication since
// it is more difficult in these cases, but by testing them against each other.

// ***********************************************************************************************

// (float)acos(numeric x)
// (float)cos(numeric x)
x = runif(10000, 0.0, PI);
xtest = acos(cos(x));
if (any(abs(xtest / x - 1.0) > 1e-5)) stop('Mismatch in test of acos() vs. cos()');

// ***********************************************************************************************

// (float)asin(numeric x)
// (float)sin(numeric x)
x = runif(10000, -PI/2, PI/2);
xtest = asin(sin(x));
if (any(abs(xtest / x - 1.0) > 1e-5)) stop('Mismatch in test of asin() vs. sin()');

// ***********************************************************************************************

// (float)atan(numeric x)
// (float)tan(numeric x)
x = runif(10000, -PI/2, PI/2);
xtest = atan(tan(x));
if (any(abs(xtest / x - 1.0) > 1e-5)) stop('Mismatch in test of atan() vs. tan()');

// ***********************************************************************************************

// (float)cos(numeric x)
// (float)sin(numeric x)
x = runif(10000, -100, 100);
xtest = sin(x)^2 + cos(x)^2;
if (any(abs(xtest - 1.0) > 1e-5)) stop('Mismatch in test of sin() vs. cos()');

// ***********************************************************************************************

// (float)atan2(numeric x, numeric y)
// (float)cos(numeric x)
// (float)sin(numeric x)
x = runif(10000, -PI, PI);
xtest = atan2(sin(x), cos(x));
if (any(abs(xtest / x - 1.0) > 1e-5)) stop('Mismatch in test of atan2() vs. sin() and cos()');

// ***********************************************************************************************

// (float)log(numeric x)
// (float)exp(numeric x)
x = runif(10000, 0.1, 100);
xtest = log(exp(x));
if (any(abs(xtest / x - 1.0) > 1e-5)) stop('Mismatch in test of log() vs. exp()');

// ***********************************************************************************************

// (float)log10(numeric x)
// operator ^
x = runif(10000, 0.1, 100);
xtest = log10(10^x);
if (any(abs(xtest / x - 1.0) > 1e-5)) stop('Mismatch in test of log10() vs. operator ^');

// ***********************************************************************************************

// (float)log2(numeric x)
// operator ^
x = runif(10000, 0.1, 100);
xtest = log2(2^x);
if (any(abs(xtest / x - 1.0) > 1e-5)) stop('Mismatch in test of log2() vs. operator ^');

// ***********************************************************************************************

// (integer)order(+ x, [logical$ ascending = T])
// (+)sort(+ x, [logical$ ascending = T])
x = sample(-1000:1000, 10000, T);		// integer
if (!identical(x[order(x)], sort(x))) stop('Mismatch in test of order(i) vs. sort(i)');
if (!identical(x[order(x, ascending=F)], sort(x, ascending=F))) stop('Mismatch in test of order(i) vs. sort(i) (ascending=F)');

x = runif(10000, 0.1, 100);		// float
if (!identical(x[order(x)], sort(x))) stop('Mismatch in test of order(f) vs. sort(f)');
if (!identical(x[order(x, ascending=F)], sort(x, ascending=F))) stop('Mismatch in test of order(f) vs. sort(f) (ascending=F)');

// ***********************************************************************************************

// (integer)match(* x, * table)
// operator []
table = -1000:1000;		// integer
x = sample(table, 10000, T);
if (!identical(table[match(x, table)], x)) stop('Mismatch in test of match(i) vs. operator []');

table = -1000.0:1000.0;		// float
x = sample(table, 10000, T);
if (!identical(table[match(x, table)], x)) stop('Mismatch in test of match(f) vs. operator []');

// ***********************************************************************************************

// (float)ceil(float x)
// (float)floor(float x)
// (float)trunc(float x)
// (float)round(float x)
x = runif(10000, -100000.0, 100000.0);
x = x[trunc(x) != x];				// eliminate numbers with no fractional part
x = x[trunc(x * 2.0) != x * 2.0];	// eliminate numbers with a fractional part of 0.5
if (any(ceil(x) != floor(x) + 1)) stop('Mismatch in test of floor() vs. ceil()');
x = abs(x);
if (any(round(x) != ifelse(x - trunc(x) < 0.5, floor(x), ceil(x)))) stop('Mismatch in test of round() vs. trunc()');

// ***********************************************************************************************

// Test random distributions against expect means; these tests aim for a fail rate of less than about 1 in 10,000
// That expected failure rate is based upon trial and error with the current Eidos, so it assumes that the present code
// is correct; it would be nice to use the theoretical variance formulas here.  But if the mean is close to the
// expected mean, the logic in Eidos is almost certainly correct, so any bug would probably be in the GSL – unlikely.

// ***********************************************************************************************

setSeed(asInteger(clock() * 100000));
m = mean(rbinom(100000, 5, 0.3));	// expectation is np, 5 * 0.3
if (abs(m - (5 * 0.3)) > 0.02) stop('Mismatch in expectation vs. realization of rbinom() - could be random chance (but very unlikely), rerun test');

// ***********************************************************************************************

// not sure how to test rcauchy() this way, since its mean, variance, etc. are undefined...

// ***********************************************************************************************

setSeed(asInteger(clock() * 100000));
x = rdunif(210000, -10, 10);
x = sapply(-10:10, "sum(x == applyValue);");
if (any(abs(x - 10000) > 500)) stop('Mismatch in expectation vs. realization of rdunif() - could be random chance (but very unlikely), rerun test');

// ***********************************************************************************************

setSeed(asInteger(clock() * 100000));
m = mean(rbeta(10000, 2, 3));	// expectation is 0.4
if (abs(m - 0.4) > 0.008) stop('Mismatch in expectation vs. realization of rbeta() - could be random chance (but very unlikely), rerun test');

// ***********************************************************************************************

setSeed(asInteger(clock() * 100000));
m = mean(rexp(100000, 5));	// expectation is 5
if (abs(m - 5) > 0.07) stop('Mismatch in expectation vs. realization of rexp() - could be random chance (but very unlikely), rerun test');

// ***********************************************************************************************

setSeed(asInteger(clock() * 100000));
m = mean(rgamma(10000, 5, 0.3));	// expectation is 5
if (abs(m - 5) > 0.4) stop('Mismatch in expectation vs. realization of rgamma() - could be random chance (but very unlikely), rerun test');

// ***********************************************************************************************

setSeed(asInteger(clock() * 100000));
m = mean(rgeom(10000, 0.1));	// expectation is 9
if (abs(m - 9) > 0.4) stop('Mismatch in expectation vs. realization of rgeom() - could be random chance (but very unlikely), rerun test');

// ***********************************************************************************************

setSeed(asInteger(clock() * 100000));
m = mean(log(rlnorm(10000, 5, 0.3)));	// expectation is 5
if (abs(m - 5) > 0.02) stop('Mismatch in expectation vs. realization of rlnorm() - could be random chance (but very unlikely), rerun test');

// ***********************************************************************************************

setSeed(asInteger(clock() * 100000));
x = rmvnorm(100000, c(-1, 5), matrix(c(1, 0.2, 0.2, 2), nrow=2));
m1 = mean(x[,0]);
m2 = mean(x[,1]);
cov = mean((x[,0] - m1) * (x[,1] - m2));
if (abs(m1 - -1.0) > 0.05) stop('Mismatch in expectation vs. realization of rmvnorm() - could be random chance (but very unlikely), rerun test');
if (abs(m2 - 5.0) > 0.07) stop('Mismatch in expectation vs. realization of rmvnorm() - could be random chance (but very unlikely), rerun test');
if (abs(cov - 0.2) > 0.03) stop('Mismatch in expectation vs. realization of rmvnorm() - could be random chance (but very unlikely), rerun test');

// ***********************************************************************************************

setSeed(asInteger(clock() * 100000));
m = mean(rnorm(10000, 5, 0.3));	// expectation is 5
if (abs(m - 5) > 0.02) stop('Mismatch in expectation vs. realization of rnorm() - could be random chance (but very unlikely), rerun test');

// ***********************************************************************************************

setSeed(asInteger(clock() * 100000));
m = mean(rpois(10000, 5));	// expectation is 5
if (abs(m - 5) > 0.09) stop('Mismatch in expectation vs. realization of rpois() - could be random chance (but very unlikely), rerun test');

// ***********************************************************************************************

setSeed(asInteger(clock() * 100000));
x = round(runif(210000, -10.4999, 10.4999));
x = sapply(-10:10, "sum(x == applyValue);");
if (any(abs(x - 10000) > 500)) stop('Mismatch in expectation vs. realization of runif() - could be random chance (but very unlikely), rerun test');

// ***********************************************************************************************

setSeed(asInteger(clock() * 100000));
m = mean(rweibull(10000, 5, 0.5));	// expectation is lambda * Gamma(1 + 1/k), and for positive integers Gamma(x) = (x-1)!, so here it is 5 * 2
if (abs(m - (5 * 2)) > 1.0) stop('Mismatch in expectation vs. realization of rweibull() - could be random chance (but very unlikely), rerun test');

// ***********************************************************************************************

setSeed(asInteger(clock() * 100000));
x = sample(-10:10, 210000, replace=T);
x = sapply(-10:10, "sum(x == applyValue);");
if (any(abs(x - 10000) > 500)) stop('Mismatch in expectation vs. realization of sample() - could be random chance (but very unlikely), rerun test');

)V0G0N"








































