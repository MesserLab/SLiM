//
//  eidos_functions.h
//  Eidos
//
//  Created by Ben Haller on 4/6/15.
//  Copyright (c) 2015-2019 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

//	This file is part of Eidos.
//
//	Eidos is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	Eidos is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with Eidos.  If not, see <http://www.gnu.org/licenses/>.

/*
 
 This file contains most of the code for processing function calls in the Eidos interpreter.
 
 */

#ifndef __Eidos__eidos_functions__
#define __Eidos__eidos_functions__

#include "eidos_interpreter.h"
#include "eidos_value.h"

#include <iostream>


// Utility functions usable by everybody
EidosValue_SP ConcatenateEidosValues(const EidosValue_SP *const p_arguments, int p_argument_count, bool p_allow_null, bool p_allow_void);
EidosValue_SP UniqueEidosValue(const EidosValue *p_value, bool p_force_new_vector, bool p_preserve_order);
EidosValue_SP Eidos_ExecuteLambdaInternal(const EidosValue_SP *const p_arguments, EidosInterpreter &p_interpreter, bool p_execute_in_outer_scope);


#pragma mark -
#pragma mark Math functions
#pragma mark -

//	math functions
EidosValue_SP Eidos_ExecuteFunction_abs(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_acos(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_asin(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_atan(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_atan2(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_ceil(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_cos(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_cumProduct(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_cumSum(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_exp(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_floor(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_integerDiv(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_integerMod(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_isFinite(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_isInfinite(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_isNAN(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_log(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_log10(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_log2(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_product(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_round(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_setDifference(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_setIntersection(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_setSymmetricDifference(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_setUnion(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_sin(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_sqrt(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_sum(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_sumExact(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_tan(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_trunc(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);


#pragma mark -
#pragma mark Statistics functions
#pragma mark -

//	statistics functions
EidosValue_SP Eidos_ExecuteFunction_cor(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_cov(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_max(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_mean(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_min(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_pmax(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_pmin(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_range(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_sd(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_ttest(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_var(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);


#pragma mark -
#pragma mark Distribution draw/density functions
#pragma mark -

//	distribution draw / density functions
EidosValue_SP Eidos_ExecuteFunction_dmvnorm(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_dnorm(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_pnorm(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rbeta(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rbinom(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rcauchy(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rdunif(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rexp(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rgamma(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rgeom(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rlnorm(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rmvnorm(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rnorm(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rpois(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_runif(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rweibull(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);


#pragma mark -
#pragma mark Vector conversion functions
#pragma mark -

//	vector construction functions
EidosValue_SP Eidos_ExecuteFunction_c(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_float(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_integer(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_logical(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_object(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rep(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_repEach(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_sample(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_seq(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_seqAlong(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_seqLen(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_string(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);


#pragma mark -
#pragma mark Value inspection/manipulation functions
#pragma mark -

//	value inspection/manipulation functions
EidosValue_SP Eidos_ExecuteFunction_all(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_any(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_cat(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_catn(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_format(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_identical(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_ifelse(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_match(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_nchar(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_order(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_paste(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_paste0(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_print(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rev(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_size_length(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_sort(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_sortBy(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_str(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_strsplit(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_substr(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_unique(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_which(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_whichMax(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_whichMin(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);


#pragma mark -
#pragma mark Value type testing/coercion functions
#pragma mark -

//	value type testing/coercion functions
EidosValue_SP Eidos_ExecuteFunction_asFloat(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_asInteger(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_asLogical(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_asString(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_elementType(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_isFloat(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_isInteger(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_isLogical(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_isNULL(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_isObject(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_isString(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_type(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);


#pragma mark -
#pragma mark Matrix and array functions
#pragma mark -

//	matrix and array functions
EidosValue_SP Eidos_ExecuteFunction_apply(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_array(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_cbind(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_dim(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_drop(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_matrix(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_matrixMult(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_ncol(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_nrow(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rbind(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_t(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);


#pragma mark -
#pragma mark Filesystem access functions
#pragma mark -

//	filesystem access functions
EidosValue_SP Eidos_ExecuteFunction_createDirectory(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_deleteFile(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_fileExists(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_filesAtPath(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_getwd(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_readFile(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_setwd(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_writeFile(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_writeTempFile(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);


#pragma mark -
#pragma mark Color manipulation functions
#pragma mark -

//	color manipulation functions
EidosValue_SP Eidos_ExecuteFunction_cmColors(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_color2rgb(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_heatColors(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_hsv2rgb(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rainbow(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rgb2color(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rgb2hsv(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_terrainColors(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);


#pragma mark -
#pragma mark Miscellaneous functions
#pragma mark -

//	miscellaneous functions
EidosValue_SP Eidos_ExecuteFunction_beep(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_citation(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_clock(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_date(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_defineConstant(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_doCall(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_executeLambda(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction__executeLambda_OUTER(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_exists(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_functionSignature(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_getSeed(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_license(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_ls(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rm(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_sapply(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_setSeed(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_stop(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_suppressWarnings(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_system(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_time(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_usage(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_version(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);


#pragma mark -
#pragma mark Object instantiation
#pragma mark -

//	object instantiation
EidosValue_SP Eidos_ExecuteFunction__Test(const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);


#endif /* defined(__Eidos__eidos_functions__) */






























































