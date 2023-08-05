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

// InteractionType -clippedIntegral() (1D x)				// EIDOS_OMPMIN_CLIPPEDINTEGRAL_1S

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
		stop("parallel InteractionType -clippedIntegral() (1D x) failed test");
}

// ***********************************************************************************************

// test InteractionType -clippedIntegral() (2D xy)			// EIDOS_OMPMIN_CLIPPEDINTEGRAL_2S

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
		stop("parallel InteractionType -clippedIntegral() (2D xy) failed test");
}

// ***********************************************************************************************

// Genome -containsMarkerMutation()							// EIDOS_OMPMIN_CONTAINS_MARKER_MUT

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

// Genome -countOfMutationsOfType()						// EIDOS_OMPMIN_G_COUNT_OF_MUTS_OF_TYPE

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

// Individual -countOfMutationsOfType()					// EIDOS_OMPMIN_I_COUNT_OF_MUTS_OF_TYPE

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

// InteractionType -drawByStrength()						// EIDOS_OMPMIN_DRAWBYSTRENGTH

initialize() {
	initializeSLiMOptions(dimensionality="xy");
	initializeInteractionType(1, "xy", reciprocal=T, maxDistance=0.15);
	i1.setInteractionFunction("f", 1.0);			// fixed interaction strength special case
}
1 late() {
	sim.addSubpop("p1", 1000000);
	p1.setSpatialBounds(c(10, 10, 100, 100));
	inds = p1.individuals;
	inds.setSpatialPosition(p1.pointUniform(p1.individualCount));
	i1.evaluate(p1);
	
	a = i1.drawByStrength(inds, returnDict=T);
	parallelSetNumThreads(1);
	b = i1.drawByStrength(inds, returnDict=T);
	
	// this test is stochastic, so we don't have a good way to test that it worked,
	// but at least we exercise the code path when running in parallel...
}

// ***********************************************************************************************

// InteractionType -drawByStrength()						// EIDOS_OMPMIN_DRAWBYSTRENGTH

initialize() {
	initializeSLiMOptions(dimensionality="xy");
	initializeInteractionType(1, "xy", reciprocal=T, maxDistance=0.15);
	i1.setInteractionFunction("n", 1.0, 0.05);		// general case
}
1 late() {
	sim.addSubpop("p1", 1000000);
	p1.setSpatialBounds(c(10, 10, 100, 100));
	inds = p1.individuals;
	inds.setSpatialPosition(p1.pointUniform(p1.individualCount));
	i1.evaluate(p1);
	
	a = i1.drawByStrength(inds, returnDict=T);
	parallelSetNumThreads(1);
	b = i1.drawByStrength(inds, returnDict=T);
	
	// this test is stochastic, so we don't have a good way to test that it worked,
	// but at least we exercise the code path when running in parallel...
}

// ***********************************************************************************************

// Species -individualsWithPedigreeIDs()					// EIDOS_OMPMIN_INDS_W_PEDIGREE_IDS

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

// InteractionType -interactingNeighborCount()				// EIDOS_OMPMIN_INTNEIGHCOUNT

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

// InteractionType -localPopulationDensity()				// EIDOS_OMPMIN_LOCALPOPDENSITY

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

// InteractionType -nearestInteractingNeighbors()			// EIDOS_OMPMIN_NEARESTINTNEIGH

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
	
	a = i1.nearestInteractingNeighbors(inds, returnDict=T);		// count=1 special-case
	parallelSetNumThreads(1);
	b = i1.nearestInteractingNeighbors(inds, returnDict=T);
	
	if (!a.identicalContents(b))
		stop("parallel InteractionType -nearestInteractingNeighbors() failed test");
}

// ***********************************************************************************************

// InteractionType -nearestInteractingNeighbors()			// EIDOS_OMPMIN_NEARESTINTNEIGH

initialize() {
	initializeSLiMOptions(dimensionality="xy");
	initializeInteractionType(1, "xy", reciprocal=T, maxDistance=0.15);
	i1.setInteractionFunction("n", 1.0, 0.05);
}
1 late() {
	sim.addSubpop("p1", 100000);
	p1.setSpatialBounds(c(10, 10, 100, 100));
	inds = p1.individuals;
	inds.setSpatialPosition(p1.pointUniform(p1.individualCount));
	i1.evaluate(p1);
	
	a = i1.nearestInteractingNeighbors(inds, count=2, returnDict=T);	// avoid the count=1 and count=N special-cases
	parallelSetNumThreads(1);
	b = i1.nearestInteractingNeighbors(inds, count=2, returnDict=T);
	
	if (!a.identicalContents(b))
		stop("parallel InteractionType -nearestInteractingNeighbors() failed test");
}

// ***********************************************************************************************

// InteractionType -nearestInteractingNeighbors()			// EIDOS_OMPMIN_NEARESTINTNEIGH

initialize() {
	initializeSLiMOptions(dimensionality="xy");
	initializeInteractionType(1, "xy", reciprocal=T, maxDistance=0.15);
	i1.setInteractionFunction("n", 1.0, 0.05);
}
1 late() {
	sim.addSubpop("p1", 1000);
	p1.setSpatialBounds(c(10, 10, 100, 100));
	inds = p1.individuals;
	inds.setSpatialPosition(p1.pointUniform(p1.individualCount));
	i1.evaluate(p1);
	
	a = i1.nearestInteractingNeighbors(inds, count=1000, returnDict=T);	// count=N special-case
	parallelSetNumThreads(1);
	b = i1.nearestInteractingNeighbors(inds, count=1000, returnDict=T);
	
	if (!a.identicalContents(b))
		stop("parallel InteractionType -nearestInteractingNeighbors() failed test");
}

// ***********************************************************************************************

// InteractionType -nearestNeighbors()						// EIDOS_OMPMIN_NEARESTNEIGH

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
	
	a = i1.nearestNeighbors(inds, returnDict=T);		// count=1 special-case
	parallelSetNumThreads(1);
	b = i1.nearestNeighbors(inds, returnDict=T);
	
	if (!a.identicalContents(b))
		stop("parallel InteractionType -nearestNeighbors() failed test");
}

// ***********************************************************************************************

// InteractionType -nearestNeighbors()						// EIDOS_OMPMIN_NEARESTNEIGH

initialize() {
	initializeSLiMOptions(dimensionality="xy");
	initializeInteractionType(1, "xy", reciprocal=T, maxDistance=0.15);
	i1.setInteractionFunction("n", 1.0, 0.05);
}
1 late() {
	sim.addSubpop("p1", 100000);
	p1.setSpatialBounds(c(10, 10, 100, 100));
	inds = p1.individuals;
	inds.setSpatialPosition(p1.pointUniform(p1.individualCount));
	i1.evaluate(p1);
	
	a = i1.nearestNeighbors(inds, count=2, returnDict=T);	// avoid the count=1 and count=N special-cases
	parallelSetNumThreads(1);
	b = i1.nearestNeighbors(inds, count=2, returnDict=T);
	
	if (!a.identicalContents(b))
		stop("parallel InteractionType -nearestNeighbors() failed test");
}

// ***********************************************************************************************

// InteractionType -nearestNeighbors()						// EIDOS_OMPMIN_NEARESTNEIGH

initialize() {
	initializeSLiMOptions(dimensionality="xy");
	initializeInteractionType(1, "xy", reciprocal=T, maxDistance=0.15);
	i1.setInteractionFunction("n", 1.0, 0.05);
}
1 late() {
	sim.addSubpop("p1", 1000);
	p1.setSpatialBounds(c(10, 10, 100, 100));
	inds = p1.individuals;
	inds.setSpatialPosition(p1.pointUniform(p1.individualCount));
	i1.evaluate(p1);
	
	a = i1.nearestNeighbors(inds, count=1000, returnDict=T);	// count=N special-cases
	parallelSetNumThreads(1);
	b = i1.nearestNeighbors(inds, count=1000, returnDict=T);
	
	if (!a.identicalContents(b))
		stop("parallel InteractionType -nearestNeighbors() failed test");
}

// ***********************************************************************************************

// InteractionType -neighborCount()							// EIDOS_OMPMIN_NEIGHCOUNT

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

// Subpopulation -pointInBounds()							// EIDOS_OMPMIN_POINT_IN_BOUNDS_1D

initialize() {
	initializeSLiMOptions(dimensionality="x");
}
1 late() {
	sim.addSubpop("p1", 100);
	p1.setSpatialBounds(c(10, 100));
	points = runif(10000000 * 1, min=-20, max=120);
	
	a = p1.pointInBounds(points);
	parallelSetNumThreads(1);
	b = p1.pointInBounds(points);
	
	if (!identical(a, b))
		stop("parallel Subpopulation -pointInBounds() (1D) failed test");
}

// ***********************************************************************************************

// Subpopulation -pointInBounds()							// EIDOS_OMPMIN_POINT_IN_BOUNDS_2D

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
		stop("parallel Subpopulation -pointInBounds() (2D) failed test");
}

// ***********************************************************************************************

// Subpopulation -pointInBounds()							// EIDOS_OMPMIN_POINT_IN_BOUNDS_3D

initialize() {
	initializeSLiMOptions(dimensionality="xyz");
}
1 late() {
	sim.addSubpop("p1", 100);
	p1.setSpatialBounds(c(10, 10, 10, 100, 100, 100));
	points = runif(10000000 * 3, min=-20, max=120);
	
	a = p1.pointInBounds(points);
	parallelSetNumThreads(1);
	b = p1.pointInBounds(points);
	
	if (!identical(a, b))
		stop("parallel Subpopulation -pointInBounds() (3D) failed test");
}

// ***********************************************************************************************

// Subpopulation -pointPeriodic()							// EIDOS_OMPMIN_POINT_PERIODIC_1D

initialize() {
	initializeSLiMOptions(dimensionality="x", periodicity="x");
}
1 late() {
	sim.addSubpop("p1", 100);
	p1.setSpatialBounds(c(0, 100));
	points = runif(10000000 * 1, min=-20, max=120);
	
	a = p1.pointPeriodic(points);
	parallelSetNumThreads(1);
	b = p1.pointPeriodic(points);
	
	if (!identical(a, b))
		stop("parallel Subpopulation -pointPeriodic() (1D) failed test");
}

// ***********************************************************************************************

// Subpopulation -pointPeriodic()							// EIDOS_OMPMIN_POINT_PERIODIC_2D

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
		stop("parallel Subpopulation -pointPeriodic() (2D) failed test");
}

// ***********************************************************************************************

// Subpopulation -pointPeriodic()							// EIDOS_OMPMIN_POINT_PERIODIC_3D

initialize() {
	initializeSLiMOptions(dimensionality="xyz", periodicity="xyz");
}
1 late() {
	sim.addSubpop("p1", 100);
	p1.setSpatialBounds(c(0, 0, 0, 100, 100, 100));
	points = runif(10000000 * 3, min=-20, max=120);
	
	a = p1.pointPeriodic(points);
	parallelSetNumThreads(1);
	b = p1.pointPeriodic(points);
	
	if (!identical(a, b))
		stop("parallel Subpopulation -pointPeriodic() (3D) failed test");
}

// ***********************************************************************************************

// Subpopulation -pointReflected()							// EIDOS_OMPMIN_POINT_REFLECTED_1D

initialize() {
	initializeSLiMOptions(dimensionality="x");
}
1 late() {
	sim.addSubpop("p1", 100);
	p1.setSpatialBounds(c(10, 100));
	points = runif(10000000 * 1, min=-20, max=120);
	
	a = p1.pointReflected(points);
	parallelSetNumThreads(1);
	b = p1.pointReflected(points);
	
	if (!identical(a, b))
		stop("parallel Subpopulation -pointReflected() (1D) failed test");
}

// ***********************************************************************************************

// Subpopulation -pointReflected()							// EIDOS_OMPMIN_POINT_REFLECTED_2D

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
		stop("parallel Subpopulation -pointReflected() (2D) failed test");
}

// ***********************************************************************************************

// Subpopulation -pointReflected()							// EIDOS_OMPMIN_POINT_REFLECTED_3D

initialize() {
	initializeSLiMOptions(dimensionality="xyz");
}
1 late() {
	sim.addSubpop("p1", 100);
	p1.setSpatialBounds(c(10, 10, 10, 100, 100, 100));
	points = runif(10000000 * 3, min=-20, max=120);
	
	a = p1.pointReflected(points);
	parallelSetNumThreads(1);
	b = p1.pointReflected(points);
	
	if (!identical(a, b))
		stop("parallel Subpopulation -pointReflected() (3D) failed test");
}

// ***********************************************************************************************

// Subpopulation -pointStopped()							// EIDOS_OMPMIN_POINT_STOPPED_1D

initialize() {
	initializeSLiMOptions(dimensionality="x");
}
1 late() {
	sim.addSubpop("p1", 100);
	p1.setSpatialBounds(c(10, 100));
	points = runif(10000000 * 1, min=-20, max=120);
	
	a = p1.pointStopped(points);
	parallelSetNumThreads(1);
	b = p1.pointStopped(points);
	
	if (!identical(a, b))
		stop("parallel Subpopulation -pointStopped() (1D) failed test");
}

// ***********************************************************************************************

// Subpopulation -pointStopped()							// EIDOS_OMPMIN_POINT_STOPPED_2D

initialize() {
	initializeSLiMOptions(dimensionality="xy");
}
1 late() {
	sim.addSubpop("p1", 100);
	p1.setSpatialBounds(c(10, 10, 100, 100));
	points = runif(10000000 * 2, min=-20, max=120);
	
	a = p1.pointStopped(points);
	parallelSetNumThreads(1);
	b = p1.pointStopped(points);
	
	if (!identical(a, b))
		stop("parallel Subpopulation -pointStopped() (2D) failed test");
}

// ***********************************************************************************************

// Subpopulation -pointStopped()							// EIDOS_OMPMIN_POINT_STOPPED_3D

initialize() {
	initializeSLiMOptions(dimensionality="xyz");
}
1 late() {
	sim.addSubpop("p1", 100);
	p1.setSpatialBounds(c(10, 10, 10, 100, 100, 100));
	points = runif(10000000 * 3, min=-20, max=120);
	
	a = p1.pointStopped(points);
	parallelSetNumThreads(1);
	b = p1.pointStopped(points);
	
	if (!identical(a, b))
		stop("parallel Subpopulation -pointStopped() (3D) failed test");
}

// ***********************************************************************************************

// Subpopulation -pointUniform()							// EIDOS_OMPMIN_POINT_UNIFORM_1D

initialize() {
	initializeSLiMOptions(dimensionality="x");
}
1 late() {
	sim.addSubpop("p1", 100);
	p1.setSpatialBounds(c(10, 100));
	
	a = p1.pointUniform(10000000);
	parallelSetNumThreads(1);
	b = p1.pointUniform(10000000);
	
	if (abs(mean(a) - mean(b)) > 0.1)
		stop("parallel Subpopulation -pointUniform() (1D) failed test");
}

// ***********************************************************************************************

// Subpopulation -pointUniform()							// EIDOS_OMPMIN_POINT_UNIFORM_2D

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
		stop("parallel Subpopulation -pointUniform() (2D) failed test");
}

// ***********************************************************************************************

// Subpopulation -pointUniform()							// EIDOS_OMPMIN_POINT_UNIFORM_3D

initialize() {
	initializeSLiMOptions(dimensionality="xyz");
}
1 late() {
	sim.addSubpop("p1", 100);
	p1.setSpatialBounds(c(10, 10, 10, 100, 100, 100));
	
	a = p1.pointUniform(10000000);
	parallelSetNumThreads(1);
	b = p1.pointUniform(10000000);
	
	if (abs(mean(a) - mean(b)) > 0.1)
		stop("parallel Subpopulation -pointUniform() (3D) failed test");
}

// ***********************************************************************************************

// Individual -relatedness()								// EIDOS_OMPMIN_RELATEDNESS

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

// Subpopulation -sampleIndividuals(replace=T)				// EIDOS_OMPMIN_SAMPLE_INDIVIDUALS_1

initialize() {
}
1 early() { sim.addSubpop("p1", 100000); }
1 late() {
	a = p1.sampleIndividuals(100000, replace=T);
	parallelSetNumThreads(1);
	b = p1.sampleIndividuals(100000, replace=T);
	
	// this test is stochastic, so we don't have a good way to test that it worked,
	// but at least we exercise the code path when running in parallel...
	if (size(a) != size(b))
		stop("parallel sampleIndividuals() 1 failed test");
}

// ***********************************************************************************************

// Subpopulation -sampleIndividuals(replace=T)				// EIDOS_OMPMIN_SAMPLE_INDIVIDUALS_2

initialize() {
}
1 early() { sim.addSubpop("p1", 100000); }
1 late() {
	a = p1.sampleIndividuals(100000, replace=T, migrant=F);
	parallelSetNumThreads(1);
	b = p1.sampleIndividuals(100000, replace=T, migrant=F);
	
	// this test is stochastic, so we don't have a good way to test that it worked,
	// but at least we exercise the code path when running in parallel...
	if (size(a) != size(b))
		stop("parallel sampleIndividuals() 2 failed test");
}

// ***********************************************************************************************

// Individual.fitnessScaling = float$						// EIDOS_OMPMIN_SET_FITNESS_SCALE_1

initialize() {
}
1 early() { sim.addSubpop("p1", 100000); }
1 late() {
	inds = p1.individuals;
	inds.fitnessScaling = 1.17;
	a = inds.fitnessScaling;
	parallelSetNumThreads(1);
	inds.fitnessScaling = 1.0;		// reset values
	inds.fitnessScaling = 1.17;
	b = inds.fitnessScaling;
	
	if (!identical(a, b))
		stop("parallel fitnessScaling = float$ failed test");
}

// ***********************************************************************************************

// Individual.fitnessScaling = float						// EIDOS_OMPMIN_SET_FITNESS_SCALE_2

initialize() {
}
1 early() { sim.addSubpop("p1", 100000); }
1 late() {
	values = runif(100000);
	inds = p1.individuals;
	inds.fitnessScaling = values;
	a = inds.fitnessScaling;
	parallelSetNumThreads(1);
	inds.fitnessScaling = 1.0;		// reset values
	inds.fitnessScaling = values;
	b = inds.fitnessScaling;
	
	if (!identical(a, b))
		stop("parallel fitnessScaling = float failed test");
}

// ***********************************************************************************************

// Individual -setSpatialPosition() 						// EIDOS_OMPMIN_SET_SPATIAL_POS_1_1D

initialize() {
	initializeSLiMOptions(dimensionality="x");
}
1 late() {
	sim.addSubpop("p1", 1000000);
	p1.setSpatialBounds(c(10, 100));
	point = p1.pointUniform(1);
	inds = p1.individuals;
	
	inds.setSpatialPosition(point);
	a = inds.spatialPosition;
	parallelSetNumThreads(1);
	inds.setSpatialPosition(point);
	b = inds.spatialPosition;
	
	if (!identical(a, b))
		stop("parallel Individual -setSpatialPosition() (1 1D) failed test");
}

// ***********************************************************************************************

// Individual -setSpatialPosition() 						// EIDOS_OMPMIN_SET_SPATIAL_POS_1_2D

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
		stop("parallel Individual -setSpatialPosition() (1 2D) failed test");
}

// ***********************************************************************************************

// Individual -setSpatialPosition() 						// EIDOS_OMPMIN_SET_SPATIAL_POS_1_3D

initialize() {
	initializeSLiMOptions(dimensionality="xyz");
}
1 late() {
	sim.addSubpop("p1", 1000000);
	p1.setSpatialBounds(c(10, 10, 10, 100, 100, 100));
	point = p1.pointUniform(1);
	inds = p1.individuals;
	
	inds.setSpatialPosition(point);
	a = inds.spatialPosition;
	parallelSetNumThreads(1);
	inds.setSpatialPosition(point);
	b = inds.spatialPosition;
	
	if (!identical(a, b))
		stop("parallel Individual -setSpatialPosition() (1 3D) failed test");
}

// ***********************************************************************************************

// Individual -setSpatialPosition() 						// EIDOS_OMPMIN_SET_SPATIAL_POS_2_1D

initialize() {
	initializeSLiMOptions(dimensionality="x");
}
1 late() {
	sim.addSubpop("p1", 1000000);
	p1.setSpatialBounds(c(10, 100));
	points = p1.pointUniform(p1.individualCount);
	inds = p1.individuals;
	
	inds.setSpatialPosition(points);
	a = inds.spatialPosition;
	parallelSetNumThreads(1);
	inds.setSpatialPosition(points);
	b = inds.spatialPosition;
	
	if (!identical(a, b))
		stop("parallel Individual -setSpatialPosition() (2 1D) failed test");
}

// ***********************************************************************************************

// Individual -setSpatialPosition() 						// EIDOS_OMPMIN_SET_SPATIAL_POS_2_2D

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
		stop("parallel Individual -setSpatialPosition() (2 2D) failed test");
}

// ***********************************************************************************************

// Individual -setSpatialPosition() 						// EIDOS_OMPMIN_SET_SPATIAL_POS_2_3D

initialize() {
	initializeSLiMOptions(dimensionality="xyz");
}
1 late() {
	sim.addSubpop("p1", 1000000);
	p1.setSpatialBounds(c(10, 10, 10, 100, 100, 100));
	points = p1.pointUniform(p1.individualCount);
	inds = p1.individuals;
	
	inds.setSpatialPosition(points);
	a = inds.spatialPosition;
	parallelSetNumThreads(1);
	inds.setSpatialPosition(points);
	b = inds.spatialPosition;
	
	if (!identical(a, b))
		stop("parallel Individual -setSpatialPosition() (2 3D) failed test");
}

// ***********************************************************************************************

// Subpopulation -spatialMapValue()							// EIDOS_OMPMIN_SPATIAL_MAP_VALUE

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

// Individual -sumOfMutationsOfType()						// EIDOS_OMPMIN_SUM_OF_MUTS_OF_TYPE

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

// InteractionType -totalOfNeighborStrengths()				// EIDOS_OMPMIN_TOTNEIGHSTRENGTH

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
