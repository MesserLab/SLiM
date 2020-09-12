//
//  eidos_functions.h
//  Eidos
//
//  Created by Ben Haller on 4/6/15.
//  Copyright (c) 2015-2020 Philipp Messer.  All rights reserved.
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
bool IdenticalEidosValues(EidosValue *x_value, EidosValue *y_value, bool p_compare_dimensions = true);
EidosValue_SP ConcatenateEidosValues(const std::vector<EidosValue_SP> &p_arguments, bool p_allow_null, bool p_allow_void);
EidosValue_SP UniqueEidosValue(const EidosValue *p_value, bool p_force_new_vector, bool p_preserve_order);
EidosValue_SP Eidos_ExecuteLambdaInternal(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter, bool p_execute_in_outer_scope);


#pragma mark -
#pragma mark Math functions
#pragma mark -

//	math functions
EidosValue_SP Eidos_ExecuteFunction_abs(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_acos(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_asin(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_atan(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_atan2(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_ceil(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_cos(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_cumProduct(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_cumSum(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_exp(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_floor(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_integerDiv(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_integerMod(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_isFinite(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_isInfinite(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_isNAN(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_log(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_log10(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_log2(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_product(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_round(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_setDifference(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_setIntersection(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_setSymmetricDifference(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_setUnion(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_sin(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_sqrt(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_sum(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_sumExact(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_tan(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_trunc(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);


#pragma mark -
#pragma mark Statistics functions
#pragma mark -

//	statistics functions
EidosValue_SP Eidos_ExecuteFunction_cor(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_cov(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_max(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_mean(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_min(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_pmax(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_pmin(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_quantile(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_range(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_sd(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_ttest(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_var(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);


#pragma mark -
#pragma mark Distribution draw/density functions
#pragma mark -

//	distribution draw / density functions
EidosValue_SP Eidos_ExecuteFunction_dmvnorm(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_dnorm(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_qnorm(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_pnorm(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_dbeta(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rbeta(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rbinom(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rcauchy(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rdunif(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_dexp(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rexp(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_dgamma(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rgamma(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rgeom(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rlnorm(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rmvnorm(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rnorm(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rpois(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_runif(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rweibull(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);


#pragma mark -
#pragma mark Vector conversion functions
#pragma mark -

//	vector construction functions
EidosValue_SP Eidos_ExecuteFunction_c(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_float(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_integer(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_logical(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_object(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rep(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_repEach(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_sample(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_seq(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_seqAlong(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_seqLen(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_string(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);


#pragma mark -
#pragma mark Value inspection/manipulation functions
#pragma mark -

//	value inspection/manipulation functions
EidosValue_SP Eidos_ExecuteFunction_all(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_any(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_cat(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_catn(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_format(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_identical(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_ifelse(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_match(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_nchar(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_order(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_paste(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_paste0(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_print(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rev(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_size_length(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_sort(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_sortBy(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_str(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_strsplit(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_substr(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_tabulate(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_unique(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_which(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_whichMax(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_whichMin(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);


#pragma mark -
#pragma mark Value type testing/coercion functions
#pragma mark -

//	value type testing/coercion functions
EidosValue_SP Eidos_ExecuteFunction_asFloat(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_asInteger(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_asLogical(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_asString(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_elementType(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_isFloat(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_isInteger(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_isLogical(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_isNULL(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_isObject(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_isString(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_type(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);


#pragma mark -
#pragma mark Matrix and array functions
#pragma mark -

//	matrix and array functions
EidosValue_SP Eidos_ExecuteFunction_apply(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_array(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_cbind(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_dim(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_drop(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_matrix(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_matrixMult(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_ncol(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_nrow(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rbind(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_t(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);


#pragma mark -
#pragma mark Filesystem access functions
#pragma mark -

//	filesystem access functions
EidosValue_SP Eidos_ExecuteFunction_createDirectory(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_deleteFile(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_fileExists(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_filesAtPath(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_getwd(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_readFile(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_setwd(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_writeFile(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_writeTempFile(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);


#pragma mark -
#pragma mark Color manipulation functions
#pragma mark -

//	color manipulation functions
EidosValue_SP Eidos_ExecuteFunction_cmColors(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_colors(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_color2rgb(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_heatColors(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_hsv2rgb(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rainbow(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rgb2color(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rgb2hsv(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_terrainColors(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);


#pragma mark -
#pragma mark Miscellaneous functions
#pragma mark -

//	miscellaneous functions
EidosValue_SP Eidos_ExecuteFunction_beep(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_citation(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_clock(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_date(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_defineConstant(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_doCall(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_executeLambda(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction__executeLambda_OUTER(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_exists(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_functionSignature(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_getSeed(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_license(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_ls(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_rm(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_sapply(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_setSeed(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_stop(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_suppressWarnings(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_system(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_time(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_usage(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
EidosValue_SP Eidos_ExecuteFunction_version(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);


#pragma mark -
#pragma mark Object instantiation
#pragma mark -

//	object instantiation
EidosValue_SP Eidos_ExecuteFunction__Test(const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);


#endif /* defined(__Eidos__eidos_functions__) */






























































