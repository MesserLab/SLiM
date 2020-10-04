//
//  interaction_type.h
//  SLiM
//
//  Created by Ben Haller on 2/25/17.
//  Copyright (c) 2017-2020 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

//	This file is part of SLiM.
//
//	SLiM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with SLiM.  If not, see <http://www.gnu.org/licenses/>.

/*
 
 The class InteractionType represents a type of interaction defined in the input file, such as spatial competition, phenotype-based
 resource competition, or spatial mating preference.  A particular interaction is defined by a spatiality (x, xy, xyz, or none), a
 maximum distance threshold, and callbacks that map from distance to interaction strength (while also perhaps involving genetic and
 environmental factors that modify the interaction).
 
 */

#ifndef __SLiM__interaction_type__
#define __SLiM__interaction_type__


#include <vector>
#include <string>
#include <map>

#include "eidos_value.h"
#include "eidos_symbol_table.h"
#include "slim_globals.h"
#include "slim_eidos_block.h"
#include "sparse_array.h"


class SLiMSim;
class Subpopulation;
class Individual;


extern EidosObjectClass *gSLiM_InteractionType_Class;


// This enumeration represents a type of interaction function (IF) that an
// interaction type can use to convert distances to interaction strengths
enum class IFType : char {
	kFixed = 0,
	kLinear,
	kExponential,
	kNormal,
	kCauchy
};

std::ostream& operator<<(std::ostream& p_out, IFType p_if_type);


// This class uses an internal implementation of kd-trees for fast nearest-neighbor finding.  We use the same data structure to
// save computed distances and interaction strengths.  A value of NaN is used as a placeholder to indicate that a given value
// has not yet been calculated, and we fill the data structure in lazily.  We keep one such data structure per evaluated
// subpopulation; if a subpopulation is not evaluated there is no overhead.
#define SLIM_MAX_DIMENSIONALITY		3

struct _SLiM_kdNode
{
	double x[SLIM_MAX_DIMENSIONALITY];		// the coordinates of the individual
	slim_popsize_t individual_index_;		// the index of the individual in its subpopulation
	struct _SLiM_kdNode *left;				// the index of the KDNode for the left side
	struct _SLiM_kdNode *right;				// the index of the KDNode for the right side
};
typedef struct _SLiM_kdNode SLiM_kdNode;

struct _InteractionsData
{
	// This flag is true when the interaction has been evaluated.  What that means in practice is that allocated blocks below
	// are in sync with the state of the population.  Interactions automatically become un-evaluated at the end of each offspring
	// generation phase, when the parental generation that they are based upon expires.  They then need to be re-evaluated,
	// which re-synchronizes their state with the state of the new parental generation.  If an interaction is marked as unevaluated,
	// it may still have allocated blocks, and their size will be indicated by individual_count_; but individual_count_ may no
	// longer have anything to do with the subpopulation for this InteractionsData block.  The intent of this design is to allow
	// us to avoid freeing and mallocing large blocks; we want to allocate them once and keep them for the lifetime of the model,
	// unless the subpopulation size changes (which forces a reallocation).  This is important because these blocks can be so large
	// that they are considered "large blocks" by malloc, triggering some very slow processing such as madvise() that is to be
	// avoided at all costs.
	bool evaluated_ = false, distances_calculated_ = false, strengths_calculated_ = false;
	std::vector<SLiMEidosBlock*> evaluation_interaction_callbacks_;
	
	slim_popsize_t individual_count_ = 0;	// the number of individuals managed; this will be equal to the size of the corresponding subpopulation
	slim_popsize_t first_male_index_ = 0;	// from the subpopulation's value; needed for sex-segregation handling
	slim_popsize_t kd_node_count_ = 0;		// the number of entries in the k-d tree; may be a multiple of individual_count_ due to periodicity
	
	double bounds_x1_, bounds_y1_, bounds_z1_;	// copied from the Subpopulation; the zero-bound in each dimension is guaranteed to be zero *if* the dimension is periodic
	
	double *positions_ = nullptr;			// individual_count_ * SLIM_MAX_DIMENSIONALITY entries, holding coordinate positions
	SparseArray *dist_str_ = nullptr;		// a sparse array of interaction distances/strengths between individuals, individual_count_ x individual_count_
	SLiM_kdNode *kd_nodes_ = nullptr;		// individual_count_ entries, holding the nodes of the k-d tree
	SLiM_kdNode *kd_root_ = nullptr;		// the root of the k-d tree
	
	_InteractionsData(const _InteractionsData&) = delete;					// no copying
	_InteractionsData& operator=(const _InteractionsData&) = delete;		// no copying
	_InteractionsData(_InteractionsData&&);									// move constructor, for std::map compatibility
	_InteractionsData& operator=(_InteractionsData&&);						// move assignment, for std::map compatibility
	_InteractionsData(void);												// null construction, for std::map compatibility
	
	_InteractionsData(slim_popsize_t p_individual_count, slim_popsize_t p_first_male_index);
	~_InteractionsData(void);
};
typedef struct _InteractionsData InteractionsData;


class InteractionType : public EidosDictionary
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
#ifdef SLIMGUI
public:
#else
private:
#endif
	
	SLiMSim &sim_;								// We have a reference back to our simulation, for flipping changed flags and such
	
	EidosSymbolTableEntry self_symbol_;			// for fast setup of the symbol table
	
	std::string spatiality_string_;				// can be "x", "y", "z", "xy", "xz", "yz", or "xyz"; this determines spatiality_
	int spatiality_;							// 0=none, 1=1D (x/y/z), 2=2D (xy/xz/yz), 3=3D (xyz)
	bool reciprocal_;							// if true, interaction strengths A->B == B->A
	double max_distance_;						// the maximum distance, beyond which interaction strength is assumed to be zero
	double max_distance_sq_;					// the maximum distance squared, cached for speed
	IndividualSex receiver_sex_;				// the sex of the individuals that feel the interaction
	IndividualSex exerter_sex_;					// the sex of the individuals that exert the interaction
	
	slim_usertag_t tag_value_ = SLIM_TAG_UNSET_VALUE;	// a user-defined tag value
	
	IFType if_type_;							// the interaction function (IF) to use
	double if_param1_, if_param2_;				// the parameters for that IF (not all of which may be used)
	
	bool periodic_x_ = false;					// true if this spatial coordinate is periodic, from SLiMSim
	bool periodic_y_ = false;					// these are in terms of the InteractionType's spatiality, not the simulation's dimensionality!
	bool periodic_z_ = false;
	
	std::map<slim_objectid_t, InteractionsData> data_;		// cached data for the interaction, for each subpopulation
	
	void CalculateAllDistances(Subpopulation *p_subpop);
	void CalculateAllStrengths(Subpopulation *p_subpop);
	
	double CalculateDistance(double *p_position1, double *p_position2);
	double CalculateDistanceWithPeriodicity(double *p_position1, double *p_position2, InteractionsData &p_subpop_data);
	
	double CalculateStrengthNoCallbacks(double p_distance);
	double CalculateStrengthWithCallbacks(double p_distance, Individual *p_receiver, Individual *p_exerter, Subpopulation *p_subpop, std::vector<SLiMEidosBlock*> &p_interaction_callbacks);
	
	SLiM_kdNode *FindMedian_p0(SLiM_kdNode *start, SLiM_kdNode *end);
	SLiM_kdNode *FindMedian_p1(SLiM_kdNode *start, SLiM_kdNode *end);
	SLiM_kdNode *FindMedian_p2(SLiM_kdNode *start, SLiM_kdNode *end);
	SLiM_kdNode *MakeKDTree1_p0(SLiM_kdNode *t, int len);
	SLiM_kdNode *MakeKDTree2_p0(SLiM_kdNode *t, int len);
	SLiM_kdNode *MakeKDTree2_p1(SLiM_kdNode *t, int len);
	SLiM_kdNode *MakeKDTree3_p0(SLiM_kdNode *t, int len);
	SLiM_kdNode *MakeKDTree3_p1(SLiM_kdNode *t, int len);
	SLiM_kdNode *MakeKDTree3_p2(SLiM_kdNode *t, int len);
	void EnsureKDTreePresent(InteractionsData &p_subpop_data);
	
	int CheckKDTree1_p0(SLiM_kdNode *t);
	void CheckKDTree1_p0_r(SLiM_kdNode *t, double split, bool isLeftSubtree);
	int CheckKDTree2_p0(SLiM_kdNode *t);
	void CheckKDTree2_p0_r(SLiM_kdNode *t, double split, bool isLeftSubtree);
	int CheckKDTree2_p1(SLiM_kdNode *t);
	void CheckKDTree2_p1_r(SLiM_kdNode *t, double split, bool isLeftSubtree);
	int CheckKDTree3_p0(SLiM_kdNode *t);
	void CheckKDTree3_p0_r(SLiM_kdNode *t, double split, bool isLeftSubtree);
	int CheckKDTree3_p1(SLiM_kdNode *t);
	void CheckKDTree3_p1_r(SLiM_kdNode *t, double split, bool isLeftSubtree);
	int CheckKDTree3_p2(SLiM_kdNode *t);
	void CheckKDTree3_p2_r(SLiM_kdNode *t, double split, bool isLeftSubtree);
	
	void BuildSA_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseArray *p_sparse_array);
	void BuildSA_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseArray *p_sparse_array, int p_phase);
	void BuildSA_3(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseArray *p_sparse_array, int p_phase);
	void BuildSA_SS_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseArray *p_sparse_array, int start_exerter, int after_end_exerter);
	void BuildSA_SS_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseArray *p_sparse_array, int start_exerter, int after_end_exerter, int p_phase);
	void BuildSA_SS_3(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseArray *p_sparse_array, int start_exerter, int after_end_exerter, int p_phase);
	
	void FindNeighbors1_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SLiM_kdNode **best, double *best_dist);
	void FindNeighbors1_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SLiM_kdNode **best, double *best_dist, int p_phase);
	void FindNeighbors1_3(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SLiM_kdNode **best, double *best_dist, int p_phase);
	void FindNeighborsA_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, EidosValue_Object_vector &p_result_vec, std::vector<Individual *> &p_individuals);
	void FindNeighborsA_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, EidosValue_Object_vector &p_result_vec, std::vector<Individual *> &p_individuals, int p_phase);
	void FindNeighborsA_3(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, EidosValue_Object_vector &p_result_vec, std::vector<Individual *> &p_individuals, int p_phase);
	void FindNeighborsN_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, int p_count, SLiM_kdNode **best, double *best_dist);
	void FindNeighborsN_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, int p_count, SLiM_kdNode **best, double *best_dist, int p_phase);
	void FindNeighborsN_3(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, int p_count, SLiM_kdNode **best, double *best_dist, int p_phase);
	void FindNeighbors(Subpopulation *p_subpop, InteractionsData &p_subpop_data, double *p_point, int p_count, EidosValue_Object_vector &p_result_vec, Individual *p_excluded_individual);
	
public:
	
	slim_objectid_t interaction_type_id_;		// the id by which this interaction type is indexed in the chromosome
	EidosValue_SP cached_value_inttype_id_;		// a cached value for interaction_type_id_; reset() if that changes
	
	
	InteractionType(const InteractionType&) = delete;					// no copying
	InteractionType& operator=(const InteractionType&) = delete;		// no copying
	InteractionType(void) = delete;										// no null construction
	InteractionType(SLiMSim &p_sim, slim_objectid_t p_interaction_type_id, std::string p_spatiality_string, bool p_reciprocal, double p_max_distance, IndividualSex p_receiver_sex, IndividualSex p_exerter_sex);
	~InteractionType(void);
	
	void EvaluateSubpopulation(Subpopulation *p_subpop, bool p_immediate);
	bool AnyEvaluated(void);
	void Invalidate(void);
	
	// apply interaction() callbacks to an interaction strength; the return value is the final interaction strength
	double ApplyInteractionCallbacks(Individual *p_receiver, Individual *p_exerter, Subpopulation *p_subpop, double p_strength, double p_distance, std::vector<SLiMEidosBlock*> &p_interaction_callbacks);
	
	// Memory usage tallying, for outputUsage()
	size_t MemoryUsageForKDTrees(void);
	size_t MemoryUsageForPositions(void);
	size_t MemoryUsageForSparseArrays(void);
	
	
	//
	// Eidos support
	//
	inline EidosSymbolTableEntry &SymbolTableEntry(void) { return self_symbol_; }
	
	virtual const EidosObjectClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	EidosValue_SP ExecuteMethod_distance(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_distanceToPoint(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_drawByStrength(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_evaluate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_interactingNeighborCount(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_interactionDistance(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_nearestInteractingNeighbors(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_nearestNeighbors(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_nearestNeighborsOfPoint(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setInteractionFunction(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_strength(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_totalOfNeighborStrengths(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_unevaluate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	
	// Accelerated property access; see class EidosObjectElement for comments on this mechanism
	static EidosValue *GetProperty_Accelerated_id(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size);
};


#endif /* __SLiM__interaction_type__ */



































