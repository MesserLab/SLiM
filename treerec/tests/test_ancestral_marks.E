initialize() {
    initializeSLiMModelType("nonWF");
    defineConstant("K",30);   
    setSeed(23);

    initializeSLiMOptions(treeRecordingPath="treeseq.txt");

    for(i in 1:20){
        initializeMutationType("m" + asString(i), 0.5, "f", 0.0);
    }

    initializeGenomicElementType("g1",c(m1,m2,m3,m4,m5,m6,m7,m8,m9,m10,m11,m12,m13,m14,m15,m16,m17,m18,m19,m20),rep(1.0,20));
    initializeGenomicElement(g1, 0, 9);
    initializeMutationRate(0.0);
    initializeRecombinationRate(0.2);    
}

reproduction() {
    subpop.addCrossed(individual,subpop.sampleIndividuals(1));
    return NULL;
}

1 early() {
    sim.addSubpop("p1",10);
    
    count = 0;
    for(geno in p1.individuals.genomes){
        for(pos in 0:9){    
            mut = sim.mutationTypes[count];
            geno.addNewDrawnMutation(mut,pos);            
        }
        count = count + 1;    
    }
    //  sim.outputFull();
}

early() {

    p1.fitnessScaling = K / p1.individualCount;

}

10 late() {

    inds = p1.individuals;

    for(Mut in sim.mutationTypes){
        for(Pos in 0:9){
            GenomesWithMutAtPos = NULL;
            for(Ind in inds){
                pedID = Ind.pedigreeID;
                genomeNum = 0;
                for(geno in Ind.genomes){
                    genomeID = (2 * pedID) + genomeNum;
                    if(geno.containsMarkerMutation(Mut,Pos)){
                        GenomesWithMutAtPos = c(GenomesWithMutAtPos,genomeID);
                    }
                    genomeNum = genomeNum + 1;    
                }
            }
            writeFile("TESToutput.txt", paste(Mut) + " " + paste(Pos), append=T);
            writeFile("TESToutput.txt", paste(GenomesWithMutAtPos), append=T);
        }
    }
    sim.simulationFinished();
}
