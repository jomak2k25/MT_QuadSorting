#pragma once

#include <cstdlib>
#include <time.h>

struct Particles
{
	// Constructor
	// Delete default constructor
	Particles() = delete;

	Particles(size_t MaxParticles, float ParticleRadius, float xVel, float yVel)
		: _MaxParticles(MaxParticles), _PosX((float*)calloc(MaxParticles, sizeof(float))), _PosY((float*)calloc(MaxParticles, sizeof(float))),
		_ParticleRadius(ParticleRadius), _ParticleDiameter(_ParticleRadius*2), _XVel(xVel), _YVel(yVel)
	{}

	// Move all particles to a random location within the provided range
	void RandomiseLocationsInRange(int MinX, int MaxX, int MinY, int MaxY)
	{
		for (size_t i = 0; i < _MaxParticles; ++i)
		{
			*(_PosX + i) = static_cast<float>(rand() % (MaxX - MinX) + MinX);
			*(_PosY + i) = static_cast<float>(rand() % (MaxY - MinY) + MinY);
		}
	}

	// Values;
	size_t _MaxParticles;
	float* _PosX = nullptr;
	float* _PosY = nullptr;

	// Information about the size of the particles
	float _ParticleRadius;
	float _ParticleDiameter;

	// Store the 2D velocity of the particles
	float _XVel;
	float _YVel;
};