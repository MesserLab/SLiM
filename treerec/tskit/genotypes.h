/*
 * MIT License
 *
 * Copyright (c) 2019 Tskit Developers
 * Copyright (c) 2016-2018 University of Oxford
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef TSK_GENOTYPES_H
#define TSK_GENOTYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tskit/trees.h>

#define TSK_16_BIT_GENOTYPES (1 << 0)
#define TSK_ISOLATED_NOT_MISSING (1 << 1)

typedef struct {
    const tsk_site_t *site;
    const char **alleles;
    tsk_size_t *allele_lengths;
    tsk_size_t num_alleles;
    tsk_size_t max_alleles;
    bool has_missing_data;
    union {
        int8_t *i8;
        int16_t *i16;
    } genotypes;
} tsk_variant_t;

typedef struct {
    size_t num_samples;
    size_t num_sites;
    const tsk_treeseq_t *tree_sequence;
    const tsk_id_t *samples;          /* samples being used */
    const tsk_id_t *sample_index_map; /* reverse index map being used */
    bool user_alleles;
    char *user_alleles_mem;
    size_t tree_site_index;
    int finished;
    tsk_id_t *traversal_stack;
    tsk_tree_t tree;
    tsk_flags_t options;
    tsk_variant_t variant;
    // private: the following data members are not intended to be used externally
    tsk_id_t *alt_samples; /* alternative subset of samples to use */
    tsk_id_t *alt_sample_index_map;
} tsk_vargen_t;

int tsk_vargen_init(tsk_vargen_t *self, const tsk_treeseq_t *tree_sequence,
    const tsk_id_t *samples, size_t num_samples, const char **alleles,
    tsk_flags_t options);
int tsk_vargen_next(tsk_vargen_t *self, tsk_variant_t **variant);
int tsk_vargen_free(tsk_vargen_t *self);
void tsk_vargen_print_state(const tsk_vargen_t *self, FILE *out);

#ifdef __cplusplus
}
#endif
#endif
