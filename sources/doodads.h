#ifndef doodads_h_xcfgh4b1r9frv
#define doodads_h_xcfgh4b1r9frv

#include "planets.h"

namespace unnatural
{
	struct Doodad
	{
		String name;
		uint32 instances = 0;

		String package;
		String proto;

		// requirements
		Vec2 temperature = Vec2(-Real::Infinity(), Real::Infinity());
		Vec2 precipitation = Vec2(-Real::Infinity(), Real::Infinity());
		Real probability;
		uint32 starting = 0; // number of instances of the doodad that must be near every starting position
		bool ocean = false;
		bool slope = false;
		bool buildable = false;
	};
}

#endif
