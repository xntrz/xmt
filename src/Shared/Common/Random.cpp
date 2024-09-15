#include "Random.hpp"

#pragma push_macro("new")
#pragma push_macro("delete")
#undef new
#undef delete
#include <random>
#pragma pop_macro("delete")
#pragma pop_macro("new")


void RndInitialize(void)
{
	;
};


void RndTerminate(void)
{
	;
};


/*DLLSHARED*/ int32 RndInt32(void)
{
	return RndInt32(int32_min, int32_max);
};


/*DLLSHARED*/ int32 RndInt32(int32 Begin, int32 End)
{
	thread_local std::random_device int_dev;
	thread_local std::mt19937 int_gen(std::seed_seq{ int_dev(), int_dev(), int_dev() });
	std::uniform_int_distribution<int32> dist(Begin, End);
	return dist(int_gen);
};


/*DLLSHARED*/ uint32 RndUInt32(void)
{
	return RndUInt32(uint32_min, uint32_max);
};


/*DLLSHARED*/ uint32 RndUInt32(uint32 Begin, uint32 End)
{
	thread_local std::random_device uint_dev;
	thread_local std::mt19937 uint_gen(std::seed_seq{ uint_dev(), uint_dev(), uint_dev() });
	std::uniform_int_distribution<uint32> dist(Begin, End);
	return dist(uint_gen);
};


/*DLLSHARED*/ float RndReal32(void)
{
	return RndReal32(std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
};


/*DLLSHARED*/ float RndReal32(float Begin, float End)
{
	thread_local std::random_device real_dev;
	thread_local std::mt19937 real_gen(std::seed_seq{ real_dev(), real_dev(), real_dev() });
	std::uniform_real_distribution<float> dist(Begin, End);
	return dist(real_gen);
};