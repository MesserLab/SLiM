/*
 * MIT License
 *
 * Copyright (c) 2019 Tskit Developers
 * Copyright (c) 2016-2017 University of Oxford
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

#ifndef TSK_STATS_H
#define TSK_STATS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tskit/trees.h>

typedef struct {
    tsk_tree_t *outer_tree;
    tsk_tree_t *inner_tree;
    tsk_size_t num_sites;
    int tree_changed;
    const tsk_treeseq_t *tree_sequence;
} tsk_ld_calc_t;

int tsk_ld_calc_init(tsk_ld_calc_t *self, const tsk_treeseq_t *tree_sequence);
int tsk_ld_calc_free(tsk_ld_calc_t *self);
void tsk_ld_calc_print_state(const tsk_ld_calc_t *self, FILE *out);
int tsk_ld_calc_get_r2(tsk_ld_calc_t *self, tsk_id_t a, tsk_id_t b, double *r2);
int tsk_ld_calc_get_r2_array(tsk_ld_calc_t *self, tsk_id_t a, int direction,
    tsk_size_t max_sites, double max_distance, double *r2, tsk_size_t *num_r2_values);

#ifdef __cplusplus
}
#endif
#endif
