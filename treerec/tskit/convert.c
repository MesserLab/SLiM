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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

#include <tskit/convert.h>

/* ======================================================== *
 * Newick output.
 * ======================================================== */

/* This infrastructure is left-over from an earlier more complex version
 * of this algorithm that worked over a tree sequence and cached the newick
 * subtrees, updating according to diffs. It's unclear whether this complexity
 * was of any real-world use, since newick output for large trees is pretty
 * pointless. */

typedef struct {
    size_t precision;
    tsk_flags_t options;
    char *newick;
    tsk_tree_t *tree;
} tsk_newick_converter_t;

static int
tsk_newick_converter_run(tsk_newick_converter_t *self, tsk_id_t root,
        size_t buffer_size, char *buffer)
{
    int ret = TSK_ERR_GENERIC;
    tsk_tree_t *tree = self->tree;
    tsk_id_t *stack = self->tree->stack1;
    const double *time = self->tree->tree_sequence->tables->nodes.time;
    int stack_top = 0;
    int label;
    size_t s = 0;
    int r;
    tsk_id_t u, v, w, root_parent;
    double branch_length;

    if (root < 0 || root >= (tsk_id_t) self->tree->num_nodes) {
        ret = TSK_ERR_NODE_OUT_OF_BOUNDS;
        goto out;
    }
    if (buffer == NULL) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    root_parent = tree->parent[root];
    stack[0] = root;
    u = root_parent;
    while (stack_top >= 0) {
        v = stack[stack_top];
        if (tree->left_child[v] != TSK_NULL && v != u) {
            if (s >= buffer_size) {
                ret = TSK_ERR_BUFFER_OVERFLOW;
                goto out;
            }
            buffer[s] = '(';
            s++;
            for (w = tree->right_child[v]; w != TSK_NULL; w = tree->left_sib[w]) {
                stack_top++;
                stack[stack_top] = w;
            }
        } else {
            u = tree->parent[v];
            stack_top--;
            if (tree->left_child[v] == TSK_NULL) {
                if (s >= buffer_size) {
                    ret = TSK_ERR_BUFFER_OVERFLOW;
                    goto out;
                }
                /* We do this for ms-compatability. This should be a configurable option
                 * via the flags attribute */
                label = v + 1;
                r = snprintf(buffer + s, buffer_size - s, "%d", label);
                if (r < 0) {
                    ret = TSK_ERR_IO;
                    goto out;
                }
                s += (size_t) r;
                if (s >= buffer_size) {
                    ret = TSK_ERR_BUFFER_OVERFLOW;
                    goto out;
                }
            }
            if (u != root_parent) {
                branch_length = (time[u] - time[v]);
                r = snprintf(buffer + s, buffer_size - s, ":%.*f", (int) self->precision,
                        branch_length);
                if (r < 0) {
                    ret = TSK_ERR_IO;
                    goto out;
                }
                s += (size_t) r;
                if (s >= buffer_size) {
                    ret = TSK_ERR_BUFFER_OVERFLOW;
                    goto out;
                }
                if (v == tree->right_child[u]) {
                    buffer[s] = ')';
                } else {
                    buffer[s] = ',';
                }
                s++;
            }
        }
    }
    if ((s + 1) >= buffer_size) {
        ret = TSK_ERR_BUFFER_OVERFLOW;
        goto out;
    }
    buffer[s] = ';';
    buffer[s + 1] = '\0';
    ret = 0;
out:
    return ret;
}

static int
tsk_newick_converter_init(tsk_newick_converter_t *self, tsk_tree_t *tree,
        size_t precision, tsk_flags_t options)
{
    int ret = 0;

    memset(self, 0, sizeof(tsk_newick_converter_t));
    self->precision = precision;
    self->options = options;
    self->tree = tree;
    return ret;
}

static int
tsk_newick_converter_free(tsk_newick_converter_t *TSK_UNUSED(self))
{
    return 0;
}

int
tsk_convert_newick(tsk_tree_t *tree, tsk_id_t root, size_t precision,
        tsk_flags_t options, size_t buffer_size, char *buffer)
{
    int ret = 0;
    tsk_newick_converter_t nc;

    ret = tsk_newick_converter_init(&nc, tree, precision, options);
    if (ret != 0) {
        goto out;
    }
    ret = tsk_newick_converter_run(&nc, root, buffer_size, buffer);
out:
    tsk_newick_converter_free(&nc);
    return ret;
}

/* ======================================================== *
 * VCF conversion.
 * ======================================================== */

void
tsk_vcf_converter_print_state(tsk_vcf_converter_t *self, FILE* out)
{
    fprintf(out, "VCF converter state\n");
    fprintf(out, "ploidy = %d\n", self->ploidy);
    fprintf(out, "num_samples = %d\n", (int) self->num_samples);
    fprintf(out, "contig_length = %lu\n", self->contig_length);
    fprintf(out, "num_vcf_samples = %d\n", (int) self->num_vcf_samples);
    fprintf(out, "header = %d bytes\n", (int) strlen(self->header));
    fprintf(out, "vcf_genotypes = %d bytes: %s", (int) self->vcf_genotypes_size,
            self->vcf_genotypes);
    fprintf(out, "record = %d bytes\n", (int) self->record_size);
}

static int TSK_WARN_UNUSED
tsk_vcf_converter_make_header(tsk_vcf_converter_t *self, const char *contig_id)
{
    int ret = TSK_ERR_GENERIC;
    const char *header_prefix_template =
        "##fileformat=VCFv4.2\n"
        "##source=tskit %d.%d.%d\n"
        "##FILTER=<ID=PASS,Description=\"All filters passed\">\n"
        "##contig=<ID=%s,length=%lu>\n"
        "##FORMAT=<ID=GT,Number=1,Type=String,Description=\"Genotype\">\n"
        "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\tFORMAT";
    char* header_prefix = NULL;
    const char *sample_pattern = "\tmsp_%d";
    size_t buffer_size, offset;
    uint32_t j;
    int written;

    written = snprintf(NULL, 0, header_prefix_template,
            TSK_VERSION_MAJOR, TSK_VERSION_MINOR, TSK_VERSION_PATCH,
            contig_id, self->contig_length);
    if (written < 0) {
        ret = TSK_ERR_IO;
        goto out;
    }
    buffer_size = (size_t) written + 1;
    header_prefix = malloc(buffer_size);
    if (header_prefix == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    written = snprintf(header_prefix, buffer_size, header_prefix_template,
            TSK_VERSION_MAJOR, TSK_VERSION_MINOR, TSK_VERSION_PATCH,
            contig_id, self->contig_length);
    if (written < 0) {
        ret = TSK_ERR_IO;
        goto out;
    }
    offset = buffer_size - 1;
    for (j = 0; j < self->num_vcf_samples; j++) {
        written = snprintf(NULL, 0, sample_pattern, j);
        if (written < 0) {
            ret = TSK_ERR_IO;
            goto out;
        }
        buffer_size += (size_t) written;
    }
    buffer_size += 1; /* make room for \n */
    self->header = malloc(buffer_size);
    if (self->header == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    memcpy(self->header, header_prefix, offset);
    for (j = 0; j < self->num_vcf_samples; j++) {
        written = snprintf(self->header + offset, buffer_size - offset,
                sample_pattern, j);
        if (written < 0) {
            ret = TSK_ERR_IO;
            goto out;
        }
        offset += (size_t) written;
        assert(offset < buffer_size);
    }
    self->header[buffer_size - 2] = '\n';
    self->header[buffer_size - 1] = '\0';
    ret = 0;
out:
    if (header_prefix != NULL) {
        free(header_prefix);
    }
    return ret;
}

static int TSK_WARN_UNUSED
tsk_vcf_converter_make_record(tsk_vcf_converter_t *self, const char *contig_id)
{
    int ret = TSK_ERR_GENERIC;
    unsigned int ploidy = self->ploidy;
    size_t n = self->num_vcf_samples;
    size_t j, k;

    self->vcf_genotypes_size = 2 * self->num_samples + 1;
    /* it's not worth working out exactly what size the record prefix
     * will be. 1K is plenty for us */
    self->record_size = 1024 + self->contig_id_size + self->vcf_genotypes_size;
    self->record = malloc(self->record_size);
    self->vcf_genotypes = malloc(self->vcf_genotypes_size);
    self->genotypes = malloc(self->num_samples * sizeof(char));
    if (self->record == NULL || self->vcf_genotypes == NULL
            || self->genotypes == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    memcpy(self->record, contig_id, self->contig_id_size);
    /* Set up the vcf_genotypes string. We don't want to have to put
     * in tabs and |s for every row so we insert them at the start.
     */
    for (j = 0; j < n; j++) {
        for (k = 0; k < ploidy; k++) {
            self->vcf_genotypes[2 * ploidy * j + 2 * k] = '0';
            self->vcf_genotypes[2 * ploidy * j + 2 * k + 1] = '|';
        }
        self->vcf_genotypes[2 * ploidy * (j + 1) - 1] = '\t';
    }
    self->vcf_genotypes[self->vcf_genotypes_size - 2] = '\n';
    self->vcf_genotypes[self->vcf_genotypes_size - 1] = '\0';
    ret = 0;
out:
    return ret;
}

static int TSK_WARN_UNUSED
tsk_vcf_converter_write_record(tsk_vcf_converter_t *self, tsk_variant_t *variant)
{
    int ret = TSK_ERR_GENERIC;
    int written;
    uint32_t j, k;
    size_t offset;
    unsigned int p = self->ploidy;
    /* TODO update this to use "%.*s", len, alleles[0] etc to write out the
     * alleles properly. */
    const char *template = "\t%lu\t.\tA\tT\t.\tPASS\t.\tGT\t";
    unsigned long pos = self->positions[variant->site->id];

    /* CHROM was written at init time as it is constant */
    written = snprintf(self->record + self->contig_id_size,
            self->record_size - self->contig_id_size, template, pos);
    if (written < 0) {
        ret = TSK_ERR_IO;
        goto out;
    }
    offset = self->contig_id_size + (size_t) written;

    for (j = 0; j < self->num_vcf_samples; j++) {
        for (k = 0; k < p; k++) {
            self->vcf_genotypes[2 * p * j + 2 * k] =
                (char) ('0' + variant->genotypes.u8[j * p + k]);
        }
    }
    assert(offset + self->vcf_genotypes_size < self->record_size);
    memcpy(self->record + offset, self->vcf_genotypes, self->vcf_genotypes_size);
    ret = 0;
out:
    return ret;
}

static int TSK_WARN_UNUSED
tsk_vcf_converter_convert_positions(tsk_vcf_converter_t *self, tsk_treeseq_t *tree_sequence)
{
    int ret = 0;
    unsigned long pos;
    tsk_site_t site;
    /* VCF is 1-based, so we must make sure we never have a 0 coordinate */
    unsigned long last_position = 0;
    size_t j;

    for (j = 0; j < self->num_sites; j++) {
        ret = tsk_treeseq_get_site(tree_sequence, (tsk_id_t) j, &site);
        if (ret != 0) {
            goto out;
        }
        /* FIXME: we shouldn't be doing this. Round to the nearest integer
         * instead. https://github.com/tskit-dev/tskit/issues/2 */

        /* update pos. We use a simple algorithm to ensure positions
         * are unique. */
        pos = (unsigned long) round(site.position);
        if (pos <= last_position) {
            pos = last_position + 1;
        }
        last_position = pos;
        self->positions[j] = pos;
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_vcf_converter_get_header(tsk_vcf_converter_t *self, char **header)
{
    *header = self->header;
    return 0;
}

int TSK_WARN_UNUSED
tsk_vcf_converter_next(tsk_vcf_converter_t *self, char **record)
{
    int ret = -1;
    int err;
    tsk_variant_t *variant;

    ret = tsk_vargen_next(self->vargen, &variant);
    if (ret < 0) {
        goto out;
    }
    if (ret == 1) {
        err = tsk_vcf_converter_write_record(self, variant);
        if (err != 0) {
            ret = err;
            goto out;
        }
        *record = self->record;
    }
out:
    return ret;
}

int TSK_WARN_UNUSED
tsk_vcf_converter_init(tsk_vcf_converter_t *self,
        tsk_treeseq_t *tree_sequence, unsigned int ploidy, const char *contig_id)
{
    int ret = -1;

    memset(self, 0, sizeof(tsk_vcf_converter_t));
    self->ploidy = ploidy;
    self->contig_id_size = strlen(contig_id);
    self->num_samples = tsk_treeseq_get_num_samples(tree_sequence);
    if (ploidy < 1 || self->num_samples % ploidy != 0) {
        ret = TSK_ERR_BAD_PARAM_VALUE;
        goto out;
    }
    self->num_vcf_samples = self->num_samples / self->ploidy;
    self->vargen = malloc(sizeof(tsk_vargen_t));
    if (self->vargen == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    ret = tsk_vargen_init(self->vargen, tree_sequence, NULL, 0, 0);
    if (ret != 0) {
        goto out;
    }
    self->num_sites = tsk_treeseq_get_num_sites(tree_sequence);
    self->positions = malloc(self->num_sites * sizeof(unsigned long));
    if (self->positions == NULL) {
        ret = TSK_ERR_NO_MEMORY;
        goto out;
    }
    ret = tsk_vcf_converter_convert_positions(self, tree_sequence);
    if (ret != 0) {
        goto out;
    }
    self->contig_length =
        (unsigned long) round(tsk_treeseq_get_sequence_length(tree_sequence));
    if (self->num_sites > 0) {
        self->contig_length = TSK_MAX(
            self->contig_length,
            self->positions[self->num_sites - 1]);
    }
    ret = tsk_vcf_converter_make_header(self, contig_id);
    if (ret != 0) {
        goto out;
    }
    if (tsk_treeseq_get_num_edges(tree_sequence) > 0) {
        ret = tsk_vcf_converter_make_record(self, contig_id);
        if (ret != 0) {
            goto out;
        }
    }
out:
    return ret;
}

int
tsk_vcf_converter_free(tsk_vcf_converter_t *self)
{
    tsk_safe_free(self->genotypes);
    tsk_safe_free(self->header);
    tsk_safe_free(self->vcf_genotypes);
    tsk_safe_free(self->record);
    tsk_safe_free(self->positions);
    if (self->vargen != NULL) {
        tsk_vargen_free(self->vargen);
        free(self->vargen);
    }
    return 0;
}
