# Meiotic Drive in SLiM

Meiotic drive, also known as segregation distortion, is a "selfish gene" phenomenon where an allele biases its own transmission during meiosis, increasing its frequency in the next generation beyond Mendelian expectations. This intragenomic conflict can lead to the spread of alleles that are neutral or even harmful to organismal fitness, illustrating the power of gene-level selection.

## Evolutionary Implications

Meiotic drive alleles can spread rapidly through populations, even if they reduce the survival or reproduction of individuals carrying them. This can result in evolutionary arms races, suppressors, and complex genomic dynamics. Understanding meiotic drive is crucial for studies of genome evolution, speciation, and genetic conflict.

## In This Simulation

The SLiM recipe `QtSLiM/recipes/Recipe 16.5 - Meiotic drive.txt` provides a concrete implementation of meiotic drive. The key mechanism is implemented in the `driveBreakpoints()` function:

```javascript
function (i)driveBreakpoints(o<Haplosome>$ gen1, o<Haplosome>$ gen2)
{
    // ... determine which haplosome carries the drive allele
    desiredPolarity = (runif(1) < D_prob) ? polarityI else !polarityI;
    // ... intervene in recombination to achieve desired outcome
}
```

The parameter `D_prob` (set to `0.8` in the recipe) controls the probability that the drive allele is transmitted to the gamete. When a heterozygous individual produces gametes, the drive allele has an 80% chance of being passed on, rather than the expected 50% under Mendelian inheritance.

This simulation illustrates how a selfish genetic element can bias its own transmission, providing a powerful tool for exploring the population‑genetic consequences of meiotic drive.

## Further Reading

- Burt, A., & Trivers, R. (2006). *Genes in Conflict: The Biology of Selfish Genetic Elements*. Harvard University Press.
- Lyttle, T. W. (1991). Segregation distorters. *Annual Review of Genetics*, 25(1), 511–557.
- SLiM manual: Recipes and advanced topics.