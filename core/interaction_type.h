//
//  interaction_type.h
//  SLiM
//
//  Created by Ben Haller on 2/25/17.
//  Copyright (c) 2017-2023 Philipp Messer.  All rights reserved.
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
#include "sparse_vector.h"
#include "subpopulation.h"
#include "spatial_kernel.h"


class Species;
class Subpopulation;
class Individual;
class InteractionType_Class;


extern EidosClass *gSLiM_InteractionType_Class;


// This class uses an internal implementation of kd-trees for fast nearest-neighbor finding.  We use the same data structure to
// save computed distances and interaction strengths.  A value of NaN is used as a placeholder to indicate that a given value
// has not yet been calculated, and we fill the data structure in lazily.  We keep one such data structure per evaluated
// subpopulation; if a subpopulation is not evaluated there is no overhead.
#define SLIM_MAX_DIMENSIONALITY		3

struct _SLiM_kdNode
{
	double x[SLIM_MAX_DIMENSIONALITY];		// the coordinates of the individual
	slim_popsize_t individual_index_;		// the index of the individual in its subpopulation, and into positions_
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
	bool evaluated_ = false;
	std::vector<SLiMEidosBlock*> evaluation_interaction_callbacks_;
	
	slim_popsize_t individual_count_ = 0;	// the number of individuals managed; this will be equal to the size of the corresponding subpopulation
	slim_popsize_t first_male_index_ = 0;	// from the subpopulation's value; needed for sex-segregation handling
	
	bool periodic_x_ = false;					// true if this spatial coordinate is periodic, from the evaluated Species
	bool periodic_y_ = false;					// these are in terms of the InteractionType's spatiality, not the simulation's dimensionality!
	bool periodic_z_ = false;
	
	double bounds_x1_, bounds_y1_, bounds_z1_;	// copied from the Subpopulation; the zero-bound in each dimension is guaranteed to be zero *if* the dimension is periodic
	
	// individual_count_ * SLIM_MAX_DIMENSIONALITY entries, holding coordinate positions for all subpop individuals regardless of constraints
	double *positions_ = nullptr;
	
	// BCH 10/31/2023: We now have two separate k-d trees, one containing all individuals (ALL) and one containing only individuals
	// that satisfy the exerter constraints of the interaction type (EXERTERS).  Each is constructed on demand, so probably most models
	// will only trigger the construction of one or the other; but models that exercise both facilities will now have ~2x the memory usage
	// for the k-d tree(s).  That is not typically a ton of memory anyway.  Note that receiver constraints are checked at query time,
	// whereas sex-specificity for exerters is checked at k-d tree construction time.  Individuals that do not satisfy the exerter
	// constraints are omitted from the EXERTERS k-d tree, and are thus never found/returned.  If no exerter constraints are present,
	// the EXERTERS k-d tree will share the same malloced blocks as the ALL k-d tree -- they will be the same tree.
	//
	// If a given kd_nodes_ pointer is nullptr, the tree has not yet been cached by CacheKDTreeNodes().  If that pointer is non-nullptr
	// but the kd_root_ pointer is nullptr, the tree has been cached, but it has not yet been built, OR the k-d tree has zero nodes
	// (i.e., is empty); the kd_node_count_ value can be used to distinguish these cases.  If the kd_root_ pointer is also non-nullptr,
	// the tree is built and ready to use.  This is all checked by the EnsureKDTreePresent_X() methods, which should always be called to
	// get the pointer to a k-d tree root; the pointers below should never be accessed directly by clients of the trees.
	
	// This k-d tree contains ALL subpop individuals regardless of constraints; it finds "neighbors", whether interacting or not
	SLiM_kdNode *kd_nodes_ALL_ = nullptr;		// individual_count_ entries, holding the nodes of the k-d tree
	SLiM_kdNode *kd_root_ALL_ = nullptr;		// the root of the k-d tree
	slim_popsize_t kd_node_count_ALL_ = 0;		// the number of entries in the k-d tree; may be a multiple of individual_count_ due to periodicity
	
	// This k-d tree contains only individuals satisfying the EXERTERS constraints; it finds "exerters" or "interacting neighbors"
	SLiM_kdNode *kd_nodes_EXERTERS_ = nullptr;		// up to individual_count_ entries, holding the nodes of the k-d tree
	SLiM_kdNode *kd_root_EXERTERS_ = nullptr;		// the root of the k-d tree
	slim_popsize_t kd_node_count_EXERTERS_ = 0;		// the number of entries in the k-d tree; may be greater than individual_count_ due to periodicity
	bool kd_constraints_raise_EXERTERS_ = false;	// an exerter tree cannot be constructed due to constraints; see EvaluateSubpopulation() for discussion
	
	_InteractionsData(const _InteractionsData&) = delete;					// no copying
	_InteractionsData& operator=(const _InteractionsData&) = delete;		// no copying
	_InteractionsData(_InteractionsData&&) noexcept;						// move constructor, for std::map compatibility
	_InteractionsData& operator=(_InteractionsData&&) noexcept;				// move assignment, for std::map compatibility
	_InteractionsData(void);												// null construction, for std::map compatibility
	
	_InteractionsData(slim_popsize_t p_individual_count, slim_popsize_t p_first_male_index);
	~_InteractionsData(void);
};
typedef struct _InteractionsData InteractionsData;


// This structure expresses constraints present for exerters or receivers; see the
// setConstraints() method for details.
typedef struct _InteractionConstraints {
	bool has_constraints_ = false;							// true if any constraints at all are present
	
	IndividualSex sex_ = IndividualSex::kUnspecified;		// IndividualSex::kUnspecified if unspecified
	
	bool has_nonsex_constraints_ = false;					// true if any non-sex constraints are present
	
	slim_usertag_t tag_ = SLIM_TAG_UNSET_VALUE;				// SLIM_TAG_UNSET_VALUE if unspecified
	slim_age_t min_age_ = -1, max_age_ = -1;				// -1 if unspecified; >= 0 otherwise
	int8_t migrant_ = -1;									// -1 if unspecified; 0 or 1 otherwise
	
	bool has_tagL_constraints_ = false;						// true if tagLX constraints are present
	int8_t tagL0_ = -1;										// -1 if unspecified; 0 or 1 otherwise
	int8_t tagL1_ = -1;										// -1 if unspecified; 0 or 1 otherwise
	int8_t tagL2_ = -1;										// -1 if unspecified; 0 or 1 otherwise
	int8_t tagL3_ = -1;										// -1 if unspecified; 0 or 1 otherwise
	int8_t tagL4_ = -1;										// -1 if unspecified; 0 or 1 otherwise
} InteractionConstraints;


class InteractionType : public EidosDictionaryUnretained
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
private:
	typedef EidosDictionaryUnretained super;

	static void _WarmUp(void);					// called internally at startup, do not call
	friend InteractionType_Class;				// so it can call _WarmUp() for us
	
#ifdef SLIMGUI
public:
#else
private:
#endif
	
	EidosSymbolTableEntry self_symbol_;			// for fast setup of the symbol table
	
	std::string spatiality_string_;				// can be "x", "y", "z", "xy", "xz", "yz", or "xyz"; this determines spatiality_
	int required_dimensionality_;				// the dimensionality required of all exerter/receiver subpops by our spatiality
	int spatiality_;							// 0=none, 1=1D (x/y/z), 2=2D (xy/xz/yz), 3=3D (xyz)
	bool reciprocal_;							// if true, interaction strengths A->B == B->A; NOW UNUSED
	double max_distance_;						// the maximum distance, beyond which interaction strength is assumed to be zero
	double max_distance_sq_;					// the maximum distance squared, cached for speed
	
	InteractionConstraints receiver_constraints_;	// constraints on who can be a receiver
	InteractionConstraints exerter_constraints_;	// constraints on who can be an exerter
	static bool _CheckIndividualNonSexConstraints(Individual *p_individual, InteractionConstraints &p_constraints);
	static bool _PrecheckIndividualNonSexConstraints(Individual *p_individual, InteractionConstraints &p_constraints);
	
	static inline __attribute__((always_inline)) bool CheckIndividualNonSexConstraints(Individual *p_individual, InteractionConstraints &p_constraints)
	{
		if (p_constraints.has_nonsex_constraints_)
			return _CheckIndividualNonSexConstraints(p_individual, p_constraints);
		return true;
	}
	
	static inline __attribute__((always_inline)) bool CheckIndividualConstraints(Individual *p_individual, InteractionConstraints &p_constraints)
	{
		if (p_constraints.has_constraints_)
		{
			if ((p_constraints.sex_ != IndividualSex::kUnspecified) && (p_constraints.sex_ != p_individual->sex_))
				return false;
			if (p_constraints.has_nonsex_constraints_)
				return _CheckIndividualNonSexConstraints(p_individual, p_constraints);
		}
		return true;
	}
	
	slim_usertag_t tag_value_ = SLIM_TAG_UNSET_VALUE;	// a user-defined tag value
	
	SpatialKernelType if_type_;					// the interaction function (IF) to use
	double if_param1_, if_param2_, if_param3_;	// the parameters for that IF (not all of which may be used)
	double n_2param2sq_;						// for type "n", precalculated == 2.0 * if_param2_ * if_param2_
	
	std::map<slim_objectid_t, InteractionsData> data_;		// cached data for the interaction, for each "exerter" subpopulation
	
	void _InvalidateData(InteractionsData &data);
	
	void CheckSpeciesCompatibility_Generic(Species &species);
	void CheckSpeciesCompatibility_Receiver(Species &species);
	void CheckSpeciesCompatibility_Exerter(Species &species);
	void CheckSpatialCompatibility(Subpopulation *receiver_subpop, Subpopulation *exerter_subpop);
	
	double CalculateDistance(double *p_position1, double *p_position2);
	double CalculateDistanceWithPeriodicity(double *p_position1, double *p_position2, InteractionsData &p_subpop_data);
	
	double CalculateStrengthNoCallbacks(double p_distance);
	double CalculateStrengthWithCallbacks(double p_distance, Individual *p_receiver, Individual *p_exerter, std::vector<SLiMEidosBlock*> &p_interaction_callbacks);
	
	SLiM_kdNode *FindMedian_p0(SLiM_kdNode *start, SLiM_kdNode *end);
	SLiM_kdNode *FindMedian_p1(SLiM_kdNode *start, SLiM_kdNode *end);
	SLiM_kdNode *FindMedian_p2(SLiM_kdNode *start, SLiM_kdNode *end);
	SLiM_kdNode *MakeKDTree1_p0(SLiM_kdNode *t, int len);
	SLiM_kdNode *MakeKDTree2_p0(SLiM_kdNode *t, int len);
	SLiM_kdNode *MakeKDTree2_p1(SLiM_kdNode *t, int len);
	SLiM_kdNode *MakeKDTree3_p0(SLiM_kdNode *t, int len);
	SLiM_kdNode *MakeKDTree3_p1(SLiM_kdNode *t, int len);
	SLiM_kdNode *MakeKDTree3_p2(SLiM_kdNode *t, int len);
	
	// Setting up the k-d trees now proceeds in several steps.  CacheKDTreeNodes() allocates the k-d tree buffers and copies positions and indices in, but does not
	// set up the left/right pointers -- it doesn't actually make the tree.  It is called at evaluate() time to set up the EXERTERS tree if exerter constraints
	// are set up, so that those constraints get applied to the state of the model at snapshot time.  For all other cases, it is called on demand when the tree
	// is needed.  BuildKDTree() takes the structure set up by CacheKDTreeNodes() and actually builds the tree structure recursively; it is called on demand when
	// the tree is needed.  EnsureKDTreePresent_ALL() and EnsureKDTreePresent_EXERTERS() are called when the corresponding k-d tree is actually needed, and it
	// triggers caching and building of the tree as needed.  They return a pointer to the tree root, which is all that is needed to use the tree for queries.
	// BEWARE!  Note that the EnsureKDTreePresent_X() methods will return nullptr if the requested tree contains zero nodes!  This needs to be checked!
	void CacheKDTreeNodes(Subpopulation *subpop, InteractionsData &p_subpop_data, bool p_apply_exerter_constraints, SLiM_kdNode **kd_nodes_ptr, SLiM_kdNode **kd_root_ptr, slim_popsize_t *kd_node_count_ptr);
	void BuildKDTree(InteractionsData &p_subpop_data, SLiM_kdNode **kd_nodes_ptr, SLiM_kdNode **kd_root_ptr, slim_popsize_t *kd_node_count_ptr);
	SLiM_kdNode *EnsureKDTreePresent_ALL(Subpopulation *subpop, InteractionsData &p_subpop_data);
	SLiM_kdNode *EnsureKDTreePresent_EXERTERS(Subpopulation *subpop, InteractionsData &p_subpop_data);
	
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
	
	void BuildSV_Presences_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector);
	void BuildSV_Presences_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int p_phase);
	void BuildSV_Presences_3(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int p_phase);
	
	void BuildSV_Distances_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector);
	void BuildSV_Distances_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int p_phase);
	void BuildSV_Distances_3(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int p_phase);
	
	void BuildSV_Strengths_f_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int p_phase);
	void BuildSV_Strengths_l_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int p_phase);
	void BuildSV_Strengths_e_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int p_phase);
	void BuildSV_Strengths_n_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int p_phase);
	void BuildSV_Strengths_c_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int p_phase);
	void BuildSV_Strengths_t_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int p_phase);
	
	int CountNeighbors_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index);
	int CountNeighbors_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, int p_phase);
	int CountNeighbors_3(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, int p_phase);
	
	void FindNeighbors1_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SLiM_kdNode **best, double *best_dist);
	void FindNeighbors1_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SLiM_kdNode **best, double *best_dist, int p_phase);
	void FindNeighbors1_3(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SLiM_kdNode **best, double *best_dist, int p_phase);
	void FindNeighborsA_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, EidosValue_Object_vector &p_result_vec, std::vector<Individual *> &p_individuals);
	void FindNeighborsA_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, EidosValue_Object_vector &p_result_vec, std::vector<Individual *> &p_individuals, int p_phase);
	void FindNeighborsA_3(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, EidosValue_Object_vector &p_result_vec, std::vector<Individual *> &p_individuals, int p_phase);
	void FindNeighborsN_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, int p_count, SLiM_kdNode **best, double *best_dist);
	void FindNeighborsN_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, int p_count, SLiM_kdNode **best, double *best_dist, int p_phase);
	void FindNeighborsN_3(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, int p_count, SLiM_kdNode **best, double *best_dist, int p_phase);
	void FindNeighbors(Subpopulation *p_subpop, SLiM_kdNode *kd_root, slim_popsize_t kd_node_count, double *p_point, int p_count, EidosValue_Object_vector &p_result_vec, Individual *p_excluded_individual, bool constraints_active);
	
	// this is a malloced 1D/2D/3D buffer, depending on our spatiality, that contains clipped integral values
	// for distances, for a focal individual, from 0 to max_distance_ to the nearest edge in each dimension
	// it is a cache that can be invalidated, without being deallocated, by setting the cached flag to false
	double *clipped_integral_ = nullptr;
	bool clipped_integral_valid_ = false;
	
	// A pool of unused SparseVector objects so that, once equilibrated, there is no alloc/realloc activity.  Note this is shared by all species.
	// When built multithreaded, we have per-thread sparse vector pools to avoid lock contention, but single-thread there is one pool.
	// At present we only use one SparseVector object per pool, but this design will allow new code to access multiple SparseVectors
	// simultaneously if that becomes useful for more complex functionality.  The overhead of the pools should be quite small.
#ifdef _OPENMP
	static std::vector<std::vector<SparseVector *>> s_freed_sparse_vectors_PERTHREAD;
	#if DEBUG
	static std::vector<int> s_sparse_vector_count_PERTHREAD;
	#endif
#else
	static std::vector<SparseVector *> s_freed_sparse_vectors_SINGLE;
	#if DEBUG
	static int s_sparse_vector_count_SINGLE;
	#endif
#endif
	
	static inline __attribute__((always_inline)) SparseVector *NewSparseVectorForExerterSubpop(Subpopulation *exerter_subpop, SparseVectorDataType data_type)
	{
		// Return a recycled SparseVector object, or create a new one if we have no recycled objects left.
		// Objects in the free list are not in a reuseable state yet, and must be reset; see FreeSparseVector() below.
		SparseVector *sv;
		
#ifdef _OPENMP
		// When running multithreaded, look up the per-thread SparseVector pool to use, and then use the single-threaded variable names
		int threadnum = omp_get_thread_num();
		std::vector<SparseVector *> &s_freed_sparse_vectors_SINGLE = s_freed_sparse_vectors_PERTHREAD[threadnum];
		#if DEBUG
		int &s_sparse_vector_count_SINGLE = s_sparse_vector_count_PERTHREAD[threadnum];
		#endif
#endif
		
		if (s_freed_sparse_vectors_SINGLE.size())
		{
			sv = s_freed_sparse_vectors_SINGLE.back();
			s_freed_sparse_vectors_SINGLE.pop_back();
			
			sv->Reset(exerter_subpop->parent_subpop_size_, data_type);
		}
		else
		{
#if DEBUG
			if (++s_sparse_vector_count_SINGLE > 1)
				std::cout << "new SparseVector(), s_sparse_vector_count_ == " << s_sparse_vector_count_SINGLE << "..." << std::endl;
#endif
			
			sv = new SparseVector(exerter_subpop->parent_subpop_size_);
			sv->SetDataType(data_type);
		}
		
		return sv;
	}
	
	static inline __attribute__((always_inline)) void FreeSparseVector(SparseVector *sv)
	{
#ifdef _OPENMP
		// When running multithreaded, look up the per-thread SparseVector pool to use, and then use the single-threaded variable names
		int threadnum = omp_get_thread_num();
		std::vector<SparseVector *> &s_freed_sparse_vectors_SINGLE = s_freed_sparse_vectors_PERTHREAD[threadnum];
		#if DEBUG
		int &s_sparse_vector_count_SINGLE = s_sparse_vector_count_PERTHREAD[threadnum];
		#endif
#endif
		
		// We return mutation runs to the free list without resetting them, because we do not know the ncols
		// value for their next usage.  They would hang on to their internal buffers for reuse.
		s_freed_sparse_vectors_SINGLE.emplace_back(sv);
		
#if DEBUG
		s_sparse_vector_count_SINGLE--;
#endif
	}
	
	void FillSparseVectorForReceiverPresences(SparseVector *sv, Individual *receiver, double *receiver_position, Subpopulation *exerter_subpop, SLiM_kdNode *kd_root, bool constraints_active);
	void FillSparseVectorForReceiverDistances(SparseVector *sv, Individual *receiver, double *receiver_position, Subpopulation *exerter_subpop, SLiM_kdNode *kd_root, bool constraints_active);
	void FillSparseVectorForPointDistances(SparseVector *sv, double *position, Subpopulation *exerter_subpop, SLiM_kdNode *kd_root);
	void FillSparseVectorForReceiverStrengths(SparseVector *sv, Individual *receiver, double *receiver_position, Subpopulation *exerter_subpop, SLiM_kdNode *kd_root, std::vector<SLiMEidosBlock*> &interaction_callbacks);
	
public:
	
	Community &community_;						// we know the community, but we have no focal species
	
	slim_objectid_t interaction_type_id_;		// the id by which this interaction type is indexed in the chromosome
	EidosValue_SP cached_value_inttype_id_;		// a cached value for interaction_type_id_; reset() if that changes
	
	
	InteractionType(const InteractionType&) = delete;					// no copying
	InteractionType& operator=(const InteractionType&) = delete;		// no copying
	InteractionType(void) = delete;										// no null construction
	InteractionType(Community &p_community, slim_objectid_t p_interaction_type_id, std::string p_spatiality_string, bool p_reciprocal, double p_max_distance, IndividualSex p_receiver_sex, IndividualSex p_exerter_sex);
	~InteractionType(void);
	
	void EvaluateSubpopulation(Subpopulation *p_subpop);
	bool AnyEvaluated(void);
	void Invalidate(void);
	void InvalidateForSpecies(Species *p_invalid_species);
	void InvalidateForSubpopulation(Subpopulation *p_invalid_subpop);
	
	void CacheClippedIntegral_1D(void);
	void CacheClippedIntegral_2D(void);
	double ClippedIntegral_1D(double indDistanceA1, double indDistanceA2, bool periodic_x);
	double ClippedIntegral_2D(double indDistanceA1, double indDistanceA2, double indDistanceB1, double indDistanceB2, bool periodic_x, bool periodic_y);
	
	// apply interaction() callbacks to an interaction strength; the return value is the final interaction strength
	double ApplyInteractionCallbacks(Individual *p_receiver, Individual *p_exerter, double p_strength, double p_distance, std::vector<SLiMEidosBlock*> &p_interaction_callbacks);
	
	// Memory usage tallying, for outputUsage()
	size_t MemoryUsageForKDTrees(void);
	size_t MemoryUsageForPositions(void);
	static size_t MemoryUsageForSparseVectorPool(void);
	
	static inline void DeleteSparseVectorFreeList(void)
	{
		// This is not normally used by SLiM, but it is used in the SLiM test code in order to prevent sparse vectors
		// that are allocated in one test from carrying over to later tests (which makes leak debugging a pain).
		
		THREAD_SAFETY_IN_ACTIVE_PARALLEL("InteractionType::DeleteSparseVectorFreeList(): s_freed_sparse_vectors_ change");
		
#ifdef _OPENMP
		// When running multithreaded, free all pools
		for (auto &pool : s_freed_sparse_vectors_PERTHREAD)
		{
			for (auto sv : pool)
				delete (sv);
			
			pool.clear();
		}
		
		#if DEBUG
		for (int &count : s_sparse_vector_count_PERTHREAD)
			count = 0;
		#endif
#else
		for (auto sv : s_freed_sparse_vectors_SINGLE)
			delete (sv);
		
		s_freed_sparse_vectors_SINGLE.clear();
		#if DEBUG
		s_sparse_vector_count_SINGLE = 0;
		#endif
#endif
	}
	
	//
	// Eidos support
	//
	inline EidosSymbolTableEntry &SymbolTableEntry(void) { return self_symbol_; }
	
	virtual const EidosClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	EidosValue_SP ExecuteMethod_clippedIntegral(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_distance(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_distanceFromPoint(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_drawByStrength(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_evaluate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_interactingNeighborCount(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_localPopulationDensity(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_interactionDistance(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_nearestInteractingNeighbors(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_nearestNeighbors(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_nearestNeighborsOfPoint(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_neighborCount(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_neighborCountOfPoint(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setConstraints(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setInteractionFunction(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_strength(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_testConstraints(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_totalOfNeighborStrengths(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_unevaluate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	
	// Accelerated property access; see class EidosObject for comments on this mechanism
	static EidosValue *GetProperty_Accelerated_id(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_tag(EidosObject **p_values, size_t p_values_size);
};

class InteractionType_Class : public EidosDictionaryUnretained_Class
{
private:
	typedef EidosDictionaryUnretained_Class super;

public:
	InteractionType_Class(const InteractionType_Class &p_original) = delete;	// no copy-construct
	InteractionType_Class& operator=(const InteractionType_Class&) = delete;	// no copying
	inline InteractionType_Class(const std::string &p_class_name, EidosClass *p_superclass) : super(p_class_name, p_superclass) { InteractionType::_WarmUp(); }
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
};


#endif /* __SLiM__interaction_type__ */



































