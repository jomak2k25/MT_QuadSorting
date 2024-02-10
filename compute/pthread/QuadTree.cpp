#include "QuadTree.h"


Quad::Quad(Particles* ParticleContainer, Quad* ParentQuad, QuadPool* QuadPool, size_t Capacity, 
	float l, float t, float r, float b, bool IsTopQuad, int Depth)
	:_ParticleContainer(ParticleContainer), _QuadPool(QuadPool), _ParentQuad(ParentQuad), _Capacity(Capacity),
	_l(l), _t(t), _r(r), _b(b), _Width(_r-_l), _Height(_t-_b), _ChildObjectIndices(), _IsTopQuad(IsTopQuad), _Depth(Depth)
{
	if (_Height < 0)
	{
		_Height = -_Height;
	}
};

bool Quad::AllocateChildQuads()
{
	_ChildQuads = _QuadPool->GetFourQuads();

	return _ChildQuads != nullptr;
}

void Quad::SortChildQuads()
{
	// Create Child quads
	float MidX = _l + ((_r - _l) / 2);
	float MidY = _b + ((_t - _b) / 2);

	for (unsigned i = 0; i < 4; ++i)
	{
		// Calculate boundaries of new quads
		float l, t, r, b;

		if ((i % 2) == 0)
		{
			l = _l;
			r = MidX;
		}
		else
		{
			l = MidX;
			r = _r;
		}
		if (i < 2)
		{
			t = _t;
			b = MidY;
		}
		else
		{
			t = MidY;
			b = _b;
		}

		// Initialise the quads with their boundaries and global capacity
		*(_ChildQuads + i) = Quad::Quad(_ParticleContainer, this, _QuadPool, _Capacity, l, t, r, b, false, ++_Depth);

	}
	// Populate Child Quads with objects with objects
	if (_IsTopQuad)
	{
		// Add object indices from entire particle container
		for (size_t i = 0; i < _ParticleContainer->_MaxParticles; ++i)
		{
			for (unsigned j = 0; j < 4; ++j)
			{
				if ((_ChildQuads + j)->TryAddObjectIndex(i))
				{
					// Move onto next index if it has been moved
					break;
				}
			}
		}
	}
	else
	{
		// Add object indices from child indices list
		for (size_t i = 0; i < _ChildObjectIndices.size(); ++i)
		{
			for (unsigned j = 0; j < 4; ++j)
			{
				if ((_ChildQuads + j)->TryAddObjectIndex(_ChildObjectIndices[i]))
				{
					// Move onto next index if it has been moved
					break;
				}
			}
		}
	}

	// Clear list of child indices NOTE: Would need changing if sorted objects varied in size
	_ChildObjectIndices.clear();
}

bool Quad::IsTooFull()
{
	if (_IsTopQuad)
	{
		return _ParticleContainer->_MaxParticles > _Capacity;
	}
	else
	{
		return _ChildObjectIndices.size() > _Capacity;
	}
}

bool Quad::ShouldBreak()
{
	return(_ParticleContainer->_ParticleDiameter <= _Width / 2 
		&& _ParticleContainer->_ParticleDiameter <= _Height / 2 
		&& IsTooFull());
}

bool Quad::TryAddObjectIndex(size_t index)
{
	// TODO : CHECK Y DIRECTION WHEN RENDERING WITH VULKAN
	float x = *(_ParticleContainer->_PosX + index);
	float y = *(_ParticleContainer->_PosY + index);


	if (x > _l && x < _r && y < _b && y > _t 
		&& _ParticleContainer->_ParticleDiameter <= _Width
		&& _ParticleContainer->_ParticleDiameter <= _Height)
	{
		_ChildObjectIndices.push_back(index);
		return true;
	}
	return false;
}


QuadPool::QuadPool()
	:_End(0)
{}

QuadPool::~QuadPool()
{
	for (int i = 0; i < _QuadPages.size(); ++i)
	{
		free(_QuadPages[i]);
	}
}

void QuadPool::Reset()
{
	_End = 0;
}

Quad* QuadPool::GetFourQuads()
{
	if (_End < _QuadPages.size())
	{
		return _QuadPages[_End++];
	}

	_QuadPages.emplace_back(static_cast<Quad*>(calloc(_PageSize, sizeof(Quad))));

	++_End;

	return _QuadPages[_QuadPages.size()-1];
}