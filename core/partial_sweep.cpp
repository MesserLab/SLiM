//
//  partial_sweep.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
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


#include "partial_sweep.h"


PartialSweep::PartialSweep(int p_mutation_type, int p_position, double p_target_prevalence)
{
	mutation_type_ = p_mutation_type;
	position_ = p_position;
	target_prevalence_ = p_target_prevalence;
}

std::ostream &operator<<(std::ostream &p_outstream, const PartialSweep &p_partial_sweep)
{
	p_outstream << "PartialSweep{mutation_type_ m" << p_partial_sweep.mutation_type_ << ", position_ " << p_partial_sweep.position_ << ", target_prevalence_ " << p_partial_sweep.target_prevalence_ << "}";
	
	return p_outstream;
}



































































