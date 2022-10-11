#include "Random.hpp"

#include <random>


struct Rnd_t
{
	std::random_device Device;
	std::mt19937 Generator;
};


static Rnd_t* Rnd;


void RndInitialize(void)
{
	if (!Rnd)
	{
		Rnd = new Rnd_t();
		ASSERT(Rnd);
		
		Rnd->Generator = std::mt19937(
			std::seed_seq{ Rnd->Device(), Rnd->Device(), Rnd->Device(), Rnd->Device() }
		);
	};
};


void RndTerminate(void)
{
	if (Rnd)
	{
		delete Rnd;
		Rnd = nullptr;
	};
};


int32 RndInt32(void)
{
	return RndInt32(int32_min, int32_max);
};


int32 RndInt32(int32 Begin, int32 End)
{
	std::uniform_int_distribution<int32> Dist(Begin, End);
	return Dist(Rnd->Generator);
};


uint32 RndUInt32(void)
{
	return RndUInt32(uint32_min, uint32_max);
};


uint32 RndUInt32(uint32 Begin, uint32 End)
{
	std::uniform_int_distribution<uint32> Dist(Begin, End);
	return Dist(Rnd->Generator);
};


float RndReal32(void)
{
	return RndReal32(
		std::numeric_limits<float>::min(),
		std::numeric_limits<float>::max()
	);
};


float RndReal32(float Begin, float End)
{
	std::uniform_real_distribution<float> Dist(Begin, End);
	return Dist(Rnd->Generator);
};