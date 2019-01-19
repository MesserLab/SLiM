//
//  eidos_object_pool.h
//  Eidos
//
//  Created by Ben Haller on 9/28/15.
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
 
 This is a modified version of an object pool published by Paulo Zemek at
 http://www.codeproject.com/Articles/746630/O-Object-Pool-in-Cplusplus .
 His code is published under the Code Project Open License (CPOL), available
 at http://www.codeproject.com/info/cpol10.aspx .  The CPOL is not compatible
 with the GPL.  Paulo Zemek has explicitly granted permission for his code
 to be used in Eidos and SLiM, and thus placed under the GPL as an alternative
 license.  An email granting this permission has been archived and can be
 provided upon request.  This code is therefore now under the same GPL license
 as the rest of SLiM and Eidos, as stated above.
 
 */


#ifndef __Eidos__eidos_object_pool_h
#define __Eidos__eidos_object_pool_h

#include <stdexcept>


class EidosObjectPool
{
private:
	struct _Node
	{
		void *_memory;
		size_t _capacity;
		_Node *_nextNode;
		
		_Node(size_t capacity, size_t itemSize)
		{
			if (capacity < 1)
				throw std::invalid_argument("capacity must be at least 1.");
			
			_memory = malloc(itemSize * capacity);
			if (_memory == NULL)
				throw std::bad_alloc();
			
			_capacity = capacity;
			_nextNode = NULL;
		}
		~_Node()
		{
			free(_memory);
		}
	};
	
	const size_t _itemSize;
	
	void *_nodeMemory;
	void *_firstDeleted;
	size_t _countInNode;
	size_t _nodeCapacity;
	_Node _firstNode;
	_Node *_lastNode;
	size_t _maxBlockLength;
	
	void _AllocateNewNode()
	{
		// Determine the number of chunks in the new node
		size_t size = _countInNode;
		
		if (size >= _maxBlockLength)
		{
			size = _maxBlockLength;
		}
		else
		{
			size *= 2;
			
			if (size < _countInNode)
				throw std::overflow_error("size became too big.");
			
			if (size >= _maxBlockLength)
				size = _maxBlockLength;
		}
		
		// Allocate the new node
		_Node *newNode = new _Node(size, _itemSize);
		
		// Link the new node in to our linked list
		_lastNode->_nextNode = newNode;
		_lastNode = newNode;
		
		// Set up to allocate out of the new node
		_nodeMemory = newNode->_memory;
		_countInNode = 0;
		_nodeCapacity = size;
	}
	
public:
	EidosObjectPool(const EidosObjectPool &) = delete;					// no copy-construct
	EidosObjectPool &operator=(const EidosObjectPool &) = delete;		// no copying
	
	explicit EidosObjectPool(size_t itemSize, size_t initialCapacity=1024, size_t maxBlockLength=1000000) : _itemSize(itemSize), _firstDeleted(nullptr), _countInNode(0), _nodeCapacity(initialCapacity), _firstNode(initialCapacity, itemSize), _maxBlockLength(maxBlockLength)
	{
		if (maxBlockLength < 1)
			throw std::invalid_argument("maxBlockLength must be at least 1.");
		
		_nodeMemory = _firstNode._memory;
		_lastNode = &_firstNode;
	}
	
	~EidosObjectPool()
	{
		_Node *node = _firstNode._nextNode;
		while(node)
		{
			_Node *nextNode = node->_nextNode;
			delete node;
			node = nextNode;
		}
	}
	
	size_t MemoryUsageForAllNodes(void)
	{
		size_t usage = 0;
		_Node *node = &_firstNode;
		while(node)
		{
			_Node *nextNode = node->_nextNode;
			usage += node->_capacity * _itemSize;
			node = nextNode;
		}
		return usage;
	}
	
	// usage: new (gXPool->AllocateChunk()) ObjectType(... parameters ...);
	inline __attribute__((always_inline)) void *AllocateChunk()
	{
		if (_firstDeleted)
		{
			void *result = _firstDeleted;
			_firstDeleted = *((void **)_firstDeleted);
			return result;
		}
		
		if (_countInNode >= _nodeCapacity)
			_AllocateNewNode();
		
		uint8_t *address = (uint8_t *)_nodeMemory;
		address += _countInNode * _itemSize;
		_countInNode++;
		return (void *)address;
	}
	
	// usage:
	//
	//	object->~ObjectType();
	//	gXPool->DisposeChunk(const_cast<ObjectType*>(object));
	inline __attribute__((always_inline)) void DisposeChunk(void *content)
	{
		*((void **)content) = _firstDeleted;
		_firstDeleted = content;
	}
};


#endif





















































