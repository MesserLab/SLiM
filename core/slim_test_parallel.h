R"V0G0N(


// This file is actually SLiM code!  It is run by _RunParallelSLiMTests() in slim_test.cpp.
// The purpose of it is to test SLiM code when run in parallel against the same code run
// single-threaded, to ensure that parallelization preserves results (to the extent possible).  The
// reason to make this a separate file is mostly because otherwise Xcode's indenting algorithm gets
// very confused.  Note this whole thing is one big C++ string literal.

// Note that this test file gets subdivided and run in chunks; this improves error reporting.  See
// _RunParallelSLiMTests() in slim_test.cpp.

// ***********************************************************************************************

// basic test, doesn't check anything, should always work

initialize() {
	initializeMutationRate(1e-7);
	initializeMutationType("m1", 0.5, "f", 0.0);
	initializeGenomicElementType("g1", m1, 1.0);
	initializeGenomicElement(g1, 0, 99999);
	initializeRecombinationRate(1e-8);
}
1 early() {
	sim.addSubpop("p1", 500);
}
100 late() {
}

// ***********************************************************************************************

// test InteractionType -clippedIntegral() (1D)

initialize() {
	initializeSLiMOptions(dimensionality="x");
	initializeInteractionType(1, "x", reciprocal=T, maxDistance=0.15);
	i1.setInteractionFunction("n", 1.0, 0.05);
}
1 late() {
	sim.addSubpop("p1", 1000000);
	p1.setSpatialBounds(c(10, 100));
	inds = p1.individuals;
	inds.setSpatialPosition(p1.pointUniform(p1.individualCount));
	i1.evaluate(p1);
	
	a = i1.clippedIntegral(inds);
	parallelSetNumThreads(1);
	b = i1.clippedIntegral(inds);
	
	if (!identical(a, b))
		stop("parallel InteractionType -clippedIntegral() (1D) failed test");
}

// ***********************************************************************************************

// test InteractionType -clippedIntegral() (2D)

initialize() {
	initializeSLiMOptions(dimensionality="xy");
	initializeInteractionType(1, "xy", reciprocal=T, maxDistance=0.15);
	i1.setInteractionFunction("n", 1.0, 0.05);
}
1 late() {
	sim.addSubpop("p1", 1000000);
	p1.setSpatialBounds(c(10, 10, 100, 100));
	inds = p1.individuals;
	inds.setSpatialPosition(p1.pointUniform(p1.individualCount));
	i1.evaluate(p1);
	
	a = i1.clippedIntegral(inds);
	parallelSetNumThreads(1);
	b = i1.clippedIntegral(inds);
	
	if (!identical(a, b))
		stop("parallel InteractionType -clippedIntegral() (2D) failed test");
}

// ***********************************************************************************************

// test Genome -containsMarkerMutation()

initialize() {
	initializeMutationRate(1e-6);
	initializeMutationType("m1", 0.5, "f", 0.0);       // neutral
	initializeMutationType("m2", 0.5, "n", 0.0, 0.2);  // QTLs
	m2.convertToSubstitution = F;
	initializeGenomicElementType("g1", c(m1, m2), c(1,1));
	initializeGenomicElement(g1, 0, 9999999);
	initializeRecombinationRate(1e-6);
}
mutationEffect(m2) { return 1.0; }
1 early() { sim.addSubpop("p1", 100); }
199 late() {
	sim.chromosome.setMutationRate(0.0);
	sim.chromosome.setRecombinationRate(0.0);
	p1.setCloningRate(1.0);
	p1.setSubpopulationSize(100000);
	
	// choose a mutation near 0.5 frequency to search for
	muts = sim.mutations;
	freqs = sim.mutationFrequencies(NULL);
	d_05 = abs(0.5 - freqs);
	i = whichMin(d_05);
	defineGlobal("MUT", muts[i]);
}
200 late() {
	genomes = sim.subpopulations.genomes;

	a = genomes.containsMarkerMutation(MUT.mutationType, MUT.position, returnMutation=F);
	parallelSetNumThreads(1);
	b = genomes.containsMarkerMutation(MUT.mutationType, MUT.position, returnMutation=F);
	
	if (!identical(a, b))
		stop("parallel Genome -containsMarkerMutation() failed test");
}

// ***********************************************************************************************

// test Genome -countOfMutationsOfType()

initialize() {
	initializeMutationRate(1e-3);
	initializeMutationType("m1", 0.5, "f", 0.0);
	initializeMutationType("m2", 0.5, "n", 0.1, 0.5);
	initializeGenomicElementType("g1", c(m1,m2), c(1,1));
	initializeGenomicElement(g1, 0, 999999);
	initializeRecombinationRate(1e-8);
}
1 early() {
	sim.addSubpop("p1", 500);
}
1 late() {
	a = p1.individuals.genomes.countOfMutationsOfType(m2);
	parallelSetNumThreads(1);
	c = p1.individuals.genomes.countOfMutationsOfType(m2);
	
	if (!identical(a, c))
		stop("parallel Genome -countOfMutationsOfType() failed test");
}

// ***********************************************************************************************

// test Individual -countOfMutationsOfType()

initialize() {
	initializeMutationRate(1e-3);
	initializeMutationType("m1", 0.5, "f", 0.0);
	initializeMutationType("m2", 0.5, "n", 0.1, 0.5);
	initializeGenomicElementType("g1", c(m1,m2), c(1,1));
	initializeGenomicElement(g1, 0, 999999);
	initializeRecombinationRate(1e-8);
}
1 early() {
	sim.addSubpop("p1", 500);
}
1 late() {
	a = p1.individuals.countOfMutationsOfType(m2);
	parallelSetNumThreads(1);
	c = p1.individuals.countOfMutationsOfType(m2);
	
	if (!identical(a, c))
		stop("parallel Individual -countOfMutationsOfType() failed test");
}

// ***********************************************************************************************

// test InteractionType -drawByStrength()

// This cannot be tested by comparison, since its results are stochastic.
// It gets tested by other means instead, such as ny InteractionType test suite.

// ***********************************************************************************************

// test Species -individualsWithPedigreeIDs()

initialize() {
	initializeSLiMOptions(keepPedigrees=T);
}
1 early() { sim.addSubpop("p1", 100000); }
1 late() {
	inds = p1.individuals;
	pids = inds.pedigreeID;
	lookup = sample(pids, 100000, replace=T);
	
	a = sim.individualsWithPedigreeIDs(lookup);
	parallelSetNumThreads(1);
	b = sim.individualsWithPedigreeIDs(lookup);
	
	if (!identical(a, b))
		stop("parallel Species -individualsWithPedigreeIDs() failed test");
}

// ***********************************************************************************************

// test InteractionType -interactingNeighborCount()

initialize() {
	initializeSLiMOptions(dimensionality="xy");
	initializeInteractionType(1, "xy", reciprocal=T, maxDistance=0.15);
	i1.setInteractionFunction("n", 1.0, 0.05);
}
1 late() {
	sim.addSubpop("p1", 1000000);
	p1.setSpatialBounds(c(10, 10, 100, 100));
	inds = p1.individuals;
	inds.setSpatialPosition(p1.pointUniform(p1.individualCount));
	i1.evaluate(p1);
	
	a = i1.interactingNeighborCount(inds);
	parallelSetNumThreads(1);
	b = i1.interactingNeighborCount(inds);
	
	if (!identical(a, b))
		stop("parallel InteractionType -interactingNeighborCount() failed test");
}

// ***********************************************************************************************

// test InteractionType -localPopulationDensity()

initialize() {
	initializeSLiMOptions(dimensionality="xy");
	initializeInteractionType(1, "xy", reciprocal=T, maxDistance=0.15);
	i1.setInteractionFunction("n", 1.0, 0.05);
}
1 late() {
	sim.addSubpop("p1", 1000000);
	p1.setSpatialBounds(c(10, 10, 100, 100));
	inds = p1.individuals;
	inds.setSpatialPosition(p1.pointUniform(p1.individualCount));
	i1.evaluate(p1);
	
	a = i1.localPopulationDensity(inds);
	parallelSetNumThreads(1);
	b = i1.localPopulationDensity(inds);
	
	if (!identical(a, b))
		stop("parallel InteractionType -localPopulationDensity() failed test");
}

// ***********************************************************************************************

// test InteractionType -nearestInteractingNeighbors()

initialize() {
	initializeSLiMOptions(dimensionality="xy");
	initializeInteractionType(1, "xy", reciprocal=T, maxDistance=0.15);
	i1.setInteractionFunction("n", 1.0, 0.05);
}
1 late() {
	sim.addSubpop("p1", 1000000);
	p1.setSpatialBounds(c(10, 10, 100, 100));
	inds = p1.individuals;
	inds.setSpatialPosition(p1.pointUniform(p1.individualCount));
	i1.evaluate(p1);
	
	a = i1.nearestInteractingNeighbors(inds, returnDict=T);
	parallelSetNumThreads(1);
	b = i1.nearestInteractingNeighbors(inds, returnDict=T);
	
	if (!a.identicalContents(b))
		stop("parallel InteractionType -nearestInteractingNeighbors() failed test");
}

// ***********************************************************************************************

// test InteractionType -nearestNeighbors()

initialize() {
	initializeSLiMOptions(dimensionality="xy");
	initializeInteractionType(1, "xy", reciprocal=T, maxDistance=0.15);
	i1.setInteractionFunction("n", 1.0, 0.05);
}
1 late() {
	sim.addSubpop("p1", 1000000);
	p1.setSpatialBounds(c(10, 10, 100, 100));
	inds = p1.individuals;
	inds.setSpatialPosition(p1.pointUniform(p1.individualCount));
	i1.evaluate(p1);
	
	a = i1.nearestNeighbors(inds, returnDict=T);
	parallelSetNumThreads(1);
	b = i1.nearestNeighbors(inds, returnDict=T);
	
	if (!a.identicalContents(b))
		stop("parallel InteractionType -nearestNeighbors() failed test");
}

// ***********************************************************************************************

// test InteractionType -neighborCount()

initialize() {
	initializeSLiMOptions(dimensionality="xy");
	initializeInteractionType(1, "xy", reciprocal=T, maxDistance=0.15);
	i1.setInteractionFunction("n", 1.0, 0.05);
}
1 late() {
	sim.addSubpop("p1", 1000000);
	p1.setSpatialBounds(c(10, 10, 100, 100));
	inds = p1.individuals;
	inds.setSpatialPosition(p1.pointUniform(p1.individualCount));
	i1.evaluate(p1);
	
	a = i1.neighborCount(inds);
	parallelSetNumThreads(1);
	b = i1.neighborCount(inds);
	
	if (!identical(a, b))
		stop("parallel InteractionType -neighborCount() failed test");
}

// ***********************************************************************************************

// test Subpopulation -pointInBounds()

initialize() {
	initializeSLiMOptions(dimensionality="xy");
}
1 late() {
	sim.addSubpop("p1", 100);
	p1.setSpatialBounds(c(10, 10, 100, 100));
	points = runif(10000000 * 2, min=-20, max=120);
	
	a = p1.pointInBounds(points);
	parallelSetNumThreads(1);
	b = p1.pointInBounds(points);
	
	if (!identical(a, b))
		stop("parallel Subpopulation -pointInBounds() failed test");
}

// ***********************************************************************************************

// test Subpopulation -pointPeriodic()

initialize() {
	initializeSLiMOptions(dimensionality="xy", periodicity="xy");
}
1 late() {
	sim.addSubpop("p1", 100);
	p1.setSpatialBounds(c(0, 0, 100, 100));
	points = runif(10000000 * 2, min=-20, max=120);
	
	a = p1.pointPeriodic(points);
	parallelSetNumThreads(1);
	b = p1.pointPeriodic(points);
	
	if (!identical(a, b))
		stop("parallel Subpopulation -pointPeriodic() failed test");
}

// ***********************************************************************************************

// test Subpopulation -pointReflected()

initialize() {
	initializeSLiMOptions(dimensionality="xy");
}
1 late() {
	sim.addSubpop("p1", 100);
	p1.setSpatialBounds(c(10, 10, 100, 100));
	points = runif(10000000 * 2, min=-20, max=120);
	
	a = p1.pointReflected(points);
	parallelSetNumThreads(1);
	b = p1.pointReflected(points);
	
	if (!identical(a, b))
		stop("parallel Subpopulation -pointReflected() failed test");
}

// ***********************************************************************************************

// test Subpopulation -pointStopped()

initialize() {
	initializeSLiMOptions(dimensionality="xy");
}
1 late() {
	sim.addSubpop("p1", 100);
	p1.setSpatialBounds(c(10, 10, 100, 100));
	points = runif(10000000 * 2, min=-20, max=120);
	
	mt = parallelGetNumThreads();
	print(mt);
	
	t1 = clock("mono");
	a = p1.pointStopped(points);
	t1 = clock("mono") - t1;
	catn("frequency == " + mean(a) + " (" + t1 + " s)");
	
	parallelSetNumThreads(1);
	
	t2 = clock("mono");
	b = p1.pointStopped(points);
	t2 = clock("mono") - t2;
	catn("frequency == " + mean(b) + " (" + t2 + " s)");
	
	parallelSetNumThreads(mt);
	
	if (!identical(a, b))
		stop("parallel Subpopulation -pointStopped() failed test");
}

// ***********************************************************************************************

// test Subpopulation -pointUniform()

initialize() {
	initializeSLiMOptions(dimensionality="xy");
}
1 late() {
	sim.addSubpop("p1", 100);
	p1.setSpatialBounds(c(10, 10, 100, 100));
	
	a = p1.pointUniform(10000000);
	parallelSetNumThreads(1);
	b = p1.pointUniform(10000000);
	
	if (abs(mean(a) - mean(b)) > 0.1)
		stop("parallel Subpopulation -pointUniform() failed test");
}

// ***********************************************************************************************

// test Individual -relatedness()

initialize() {
	initializeSLiMOptions(keepPedigrees=T);
}
1 early() { sim.addSubpop("p1", 100); }
19 late() { p1.setSubpopulationSize(100000); }
20 late() {
	inds = p1.individuals;
	focalInd = inds[0];
	
	a = focalInd.relatedness(inds);
	parallelSetNumThreads(1);
	b = focalInd.relatedness(inds);
	
	if (!identical(a, b))
		stop("parallel Individual -relatedness() failed test");
}

// ***********************************************************************************************

// test Individual -setSpatialPosition() (one point across all targets)

initialize() {
	initializeSLiMOptions(dimensionality="xy");
}
1 late() {
	sim.addSubpop("p1", 1000000);
	p1.setSpatialBounds(c(10, 10, 100, 100));
	point = p1.pointUniform(1);
	inds = p1.individuals;
	
	inds.setSpatialPosition(point);
	a = inds.spatialPosition;
	parallelSetNumThreads(1);
	inds.setSpatialPosition(point);
	b = inds.spatialPosition;
	
	if (!identical(a, b))
		stop("parallel Individual -setSpatialPosition() (1) failed test");
}

// ***********************************************************************************************

// test Individual -setSpatialPosition() (one point for each target)

initialize() {
	initializeSLiMOptions(dimensionality="xy");
}
1 late() {
	sim.addSubpop("p1", 1000000);
	p1.setSpatialBounds(c(10, 10, 100, 100));
	points = p1.pointUniform(p1.individualCount);
	inds = p1.individuals;
	
	inds.setSpatialPosition(points);
	a = inds.spatialPosition;
	parallelSetNumThreads(1);
	inds.setSpatialPosition(points);
	b = inds.spatialPosition;
	
	if (!identical(a, b))
		stop("parallel Individual -setSpatialPosition() (2) failed test");
}

// ***********************************************************************************************

// test Subpopulation -spatialMapValue()

initialize() {
	initializeSLiMOptions(dimensionality="xy");
}
1 late() {
	sim.addSubpop("p1", 100);
	p1.setSpatialBounds(c(10, 10, 100, 100));
	mapValues = matrix(runif(100*100), nrow=100, ncol=100);
	p1.defineSpatialMap("map", "xy", mapValues, interpolate=T);
	points = runif(10000000 * 2, min=-20, max=120);
	
	a = p1.spatialMapValue("map", points);
	parallelSetNumThreads(1);
	b = p1.spatialMapValue("map", points);
	
	if (!identical(a, b))
		stop("parallel Subpopulation -spatialMapValue() failed test");
}

// ***********************************************************************************************

// test Individual -sumOfMutationsOfType()

initialize() {
	initializeMutationRate(1e-3);
	initializeMutationType("m1", 0.5, "f", 0.0);
	initializeMutationType("m2", 0.5, "n", 0.1, 0.5);
	initializeGenomicElementType("g1", c(m1,m2), c(1,1));
	initializeGenomicElement(g1, 0, 999999);
	initializeRecombinationRate(1e-8);
}
1 early() {
	sim.addSubpop("p1", 500);
}
1 late() {
	b = p1.individuals.sumOfMutationsOfType(m2);
	parallelSetNumThreads(1);
	d = p1.individuals.sumOfMutationsOfType(m2);
	
	if (!identical(b, d))
		stop("parallel Individual -sumOfMutationsOfType() failed test");
}

// ***********************************************************************************************

// test InteractionType -totalOfNeighborStrengths()

initialize() {
	initializeSLiMOptions(dimensionality="xy");
	initializeInteractionType(1, "xy", reciprocal=T, maxDistance=0.15);
	i1.setInteractionFunction("n", 1.0, 0.05);
}
1 late() {
	sim.addSubpop("p1", 1000000);
	p1.setSpatialBounds(c(10, 10, 100, 100));
	inds = p1.individuals;
	inds.setSpatialPosition(p1.pointUniform(p1.individualCount));
	i1.evaluate(p1);
	
	a = i1.totalOfNeighborStrengths(inds);
	parallelSetNumThreads(1);
	b = i1.totalOfNeighborStrengths(inds);
	
	if (!identical(a, b))
		stop("parallel InteractionType -totalOfNeighborStrengths() failed test");
}

)V0G0N"
