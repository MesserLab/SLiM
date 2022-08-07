/*
 * MIT License
 *
 * Copyright (c) 2019-2022 Tskit Developers
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

#define TSK_ISOLATED_NOT_MISSING (1 << 1)

/**
@brief A variant at a specific site.

@rst
Used to generate the genotypes for a given set of samples at a given
site.
@endrst
*/
typedef struct {
    /** @brief Unowned reference to the tree sequence of the variant */
    const tsk_treeseq_t *tree_sequence;
    /** @brief The site this variant is currently decoded at*/
    tsk_site_t site;
    tsk_tree_t tree;
    /** @brief Array of allele strings that the genotypes of the variant refer to
     *  These are not NULL terminated - use `allele_lengths` for example:.
     *  `printf("%.*s", (int) var->allele_lengths[j], var->alleles[j]);`
     */
    const char **alleles;
    /** @brief Lengths of the allele strings */
    tsk_size_t *allele_lengths;
    /** @brief Length of the allele array */
    tsk_size_t num_alleles;
    tsk_size_t max_alleles;
    /** @brief If True the genotypes of isolated nodes have been decoded to the "missing"
     * genotype. If False they are set to the ancestral state (in the absence of
     * mutations above them)*/
    bool has_missing_data;
    /** @brief Array of genotypes for the current site */
    int32_t *genotypes;
    /** @brief Number of samples */
    tsk_size_t num_samples;
    /** @brief Array of sample ids used*/
    tsk_id_t *samples;

    const tsk_id_t *sample_index_map;
    bool user_alleles;
    char *user_alleles_mem;
    tsk_id_t *traversal_stack;
    tsk_flags_t options;
    tsk_id_t *alt_samples;
    tsk_id_t *alt_sample_index_map;

} tsk_variant_t;

/* All vargen related structs and methods were deprecated in C API v1.0 */
typedef struct {
    const tsk_treeseq_t *tree_sequence;
    tsk_id_t site_index;
    tsk_variant_t variant;
} tsk_vargen_t;

/**
@defgroup VARIANT_API_GROUP Variant API for obtaining genotypes.
@{
*/

/**
@brief Initialises the variant by allocating the internal memory

@rst
This must be called before any operations are performed on the variant.
See the :ref:`sec_c_api_overview_structure` for details on how objects
are initialised and freed.
@endrst

@param self A pointer to an uninitialised tsk_variant_t object.
@param tree_sequence A pointer to the tree sequence from which this variant
will decode genotypes. No copy is taken, so this tree sequence must persist
for the lifetime of the variant.
@param samples Optional. Either `NULL` or an array of node ids of the samples that are to
have their genotypes decoded. A copy of this array will be taken by the variant. If
`NULL` then the samples from the tree sequence will be used.
@param num_samples The number of ids in the samples array, ignored if `samples` is `NULL`
@param alleles Optional. Either ``NULL`` or an array of string alleles with a terminal
``NULL`` sentinel value.
If specified, the genotypes will be decoded to match the index in this allele array.
If ``NULL`` then alleles will be automatically determined from the mutations encountered.
@param options Variant options. Either ``0`` or ``TSK_ISOLATED_NOT_MISSING`` which
if specified indicates that isolated sample nodes should not be decoded as the "missing"
state but as the ancestral state (or the state of any mutation above them).
@return Return 0 on success or a negative value on failure.
*/
int tsk_variant_init(tsk_variant_t *self, const tsk_treeseq_t *tree_sequence,
    const tsk_id_t *samples, tsk_size_t num_samples, const char **alleles,
    tsk_flags_t options);

/**
@brief Copies the state of this variant to another variant

@rst
Copies the site, genotypes and alleles from this variant to another. Note that
the other variant should be uninitialised as this method does not free any
memory that the other variant owns. After copying `other` is frozen and
this restricts it from being further decoded at any site. `self` remains unchanged.
@endrst

@param self A pointer to an initialised and decoded tsk_variant_t object.
@param other A pointer to an uninitialised tsk_variant_t object.
@return Return 0 on success or a negative value on failure.
*/
int tsk_variant_restricted_copy(const tsk_variant_t *self, tsk_variant_t *other);

/**
@brief Decode the genotypes at the given site, storing them in this variant.

@rst
Decodes the genotypes for this variant's samples, indexed to this variant's alleles,
at the specified site.
This method is most efficient at decoding sites in-order, either forwards or backwards
along the tree sequence. Resulting genotypes are stored in the ``genotypes`` member of
this variant.
@endrst

@param self A pointer to an initialised tsk_variant_t object.
@param site_id A valid site id for the tree sequence of this variant.
@param options Bitwise option flags. Currently unused; should be
    set to zero to ensure compatibility with later versions of `tskit`.
@return Return 0 on success or a negative value on failure.
*/
int tsk_variant_decode(tsk_variant_t *self, tsk_id_t site_id, tsk_flags_t options);

/**
@brief Free the internal memory for the specified variant.

@param self A pointer to an initialised tsk_variant_t object.
@return Always returns 0.
*/
int tsk_variant_free(tsk_variant_t *self);

/**
@brief Print out the state of this variant to the specified stream.

This method is intended for debugging purposes and should not be used
in production code. The format of the output should **not** be depended
on and may change arbitrarily between versions.

@param self A pointer to a tsk_variant_t object.
@param out The stream to write the summary to.
*/
void tsk_variant_print_state(const tsk_variant_t *self, FILE *out);

/** @} */

/* Deprecated vargen methods (since C API v1.0) */
int tsk_vargen_init(tsk_vargen_t *self, const tsk_treeseq_t *tree_sequence,
    const tsk_id_t *samples, tsk_size_t num_samples, const char **alleles,
    tsk_flags_t options);
int tsk_vargen_next(tsk_vargen_t *self, tsk_variant_t **variant);
int tsk_vargen_free(tsk_vargen_t *self);
void tsk_vargen_print_state(const tsk_vargen_t *self, FILE *out);

#ifdef __cplusplus
}
#endif
#endif
