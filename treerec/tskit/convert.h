/*
 * MIT License
 *
 * Copyright (c) 2019 Tskit Developers
 * Copyright (c) 2015-2017 University of Oxford
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

#ifndef TSK_CONVERT_H
#define TSK_CONVERT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tskit/genotypes.h>

/* TODO do we really need to expose this or would a simpler function be
 * more appropriate? Depends on how we use it at the Python level probably. */

typedef struct {
    size_t num_samples;
    size_t num_vcf_samples;
    unsigned int ploidy;
    char *genotypes;
    char *header;
    char *record;
    char *vcf_genotypes;
    size_t vcf_genotypes_size;
    size_t contig_id_size;
    size_t record_size;
    size_t num_sites;
    unsigned long contig_length;
    unsigned long *positions;
    tsk_vargen_t *vargen;
} tsk_vcf_converter_t;

int tsk_vcf_converter_init(tsk_vcf_converter_t *self,
        tsk_treeseq_t *tree_sequence, unsigned int ploidy, const char *chrom);
int tsk_vcf_converter_get_header(tsk_vcf_converter_t *self, char **header);
int tsk_vcf_converter_next(tsk_vcf_converter_t *self, char **record);
int tsk_vcf_converter_free(tsk_vcf_converter_t *self);
void tsk_vcf_converter_print_state(tsk_vcf_converter_t *self, FILE *out);


int tsk_convert_newick(tsk_tree_t *tree, tsk_id_t root, size_t precision,
        tsk_flags_t options, size_t buffer_size, char *buffer);

#ifdef __cplusplus
}
#endif
#endif
