#pragma once

#define DLLEXPORT __declspec(dllexport)
#define DLLIMPORT __declspec(dllimport)

#ifdef APPLICATION_BASE
#define DLLSHARED DLLEXPORT
#else
#define DLLSHARED DLLIMPORT
#endif