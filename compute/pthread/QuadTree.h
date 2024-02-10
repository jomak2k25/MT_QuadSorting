#pragma once
#include <vector>
#include "Particle.h"

struct QuadPool;

// Quad structure which can be used in a QuadTree to sort particles
struct Quad
{
	Quad() = delete;

	// Constructor
	Quad(Particles* ParticleContainer, Quad* ParentQuad, QuadPool* QuadPool, size_t Capacity, 
		float l, float t, float r, float b, bool IsTopQuad = false, int Depth = 0);

	// Allocates Quads from the global QuadPool, is critical, returns false if allocation failed
	bool AllocateChildQuads();

	// Sorts the particles into the Child Quads,
	void SortChildQuads();

	// Check if this this Quad is over capacity
	bool IsTooFull();

	// Check if the Quad is too full and if particles would fit into child quads
	bool ShouldBreak();

	// Check if the origin of an object is within the bounds of this quad, and add it if it is
	// Return true = Is within bounds and was added
	bool TryAddObjectIndex(size_t index);

	// Pointer to the particle container storing particle data
	Particles* _ParticleContainer = nullptr;

	// Pointer to the parent quad
	Quad* _ParentQuad = nullptr;

	// Pointer to the quad pool used to get new quads allocated
	QuadPool* _QuadPool = nullptr;

	// Soft capacity of the quad
	size_t _Capacity;

	// Coordinates of the sides
	float _l, _t, _r, _b;

	float _Width, _Height;

	// Indices to particles stored inside the particle container
	std::vector<size_t> _ChildObjectIndices;

	// Flag to check if this is the Top Quad with no parent
	bool _IsTopQuad;

	// The depth in the quad tree
	int _Depth;

	// Pointer to child quads
	Quad* _ChildQuads = nullptr;
};


// Quad pool to pre-allocate memory for Quads and retrieve Quads
struct QuadPool
{
	QuadPool();

	~QuadPool();

	// Reset the QuadPool so all allocated quads become free
	void Reset();

	// Returns nullptr if there isn't any Quads Available
	Quad* GetFourQuads();

	std::vector<Quad*> _QuadPages;

	static const unsigned _PageSize = 4;

	unsigned _End;
};

