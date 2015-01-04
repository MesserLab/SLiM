//
//  event.cpp
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


#include "event.h"


Event::Event(char p_event_type, std::vector<std::string> p_parameters) : event_type_(p_event_type), parameters_(p_parameters)
{
	static std::string possible_options = "PNSMXARFT";
	
	if (possible_options.find(event_type_) == std::string::npos) 
	{ 
		std::cerr << "ERROR (Initialize): invalid event type \"" << event_type_;
		
		for (int i = 0; i < parameters_.size(); i++)
			std::cerr << " " << parameters_[i];
		
		std::cerr << "\"" << std::endl;
		exit(EXIT_FAILURE); 
	}
}  

std::ostream &operator<<(std::ostream &p_outstream, const Event &p_event)
{
	p_outstream << "Event{event_type_ '" << p_event.event_type_ << "', parameters_ ";
	
	if (p_event.parameters_.size() == 0)
	{
		p_outstream << "*";
	}
	else
	{
		p_outstream << "<";
		
		for (int i = 0; i < p_event.parameters_.size(); ++i)
		{
			p_outstream << p_event.parameters_[i];
			
			if (i < p_event.parameters_.size() - 1)
				p_outstream << " ";
		}
		
		p_outstream << ">";
	}
	
	p_outstream << "}";
	
	return p_outstream;
}



































































