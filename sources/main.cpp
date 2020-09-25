#include <cage-core/logger.h>
#include <cage-core/ini.h>
#include <cage-core/config.h>

#include "generator.h"

namespace
{
	void applyConfiguration(const Holder<Ini> &cmd)
	{
		ConfigString baseShapeName("unnatural-planets/planet/shape", "random");
		baseShapeName = cmd->cmdString('s', "shape", baseShapeName);
		updateBaseShapeFunctionPointer();

		ConfigBool useTerrainPoles("unnatural-planets/planet/poles", (string)baseShapeName == "sphere");
		useTerrainPoles = cmd->cmdBool('p', "poles", useTerrainPoles);
		CAGE_LOG(SeverityEnum::Info, "configuration", stringizer() + "using poles: " + !!useTerrainPoles);
		
		ConfigBool useTerrainElevation("unnatural-planets/planet/elevation", true);
		useTerrainElevation = cmd->cmdBool('e', "elevation", useTerrainElevation);
		CAGE_LOG(SeverityEnum::Info, "configuration", stringizer() + "using terrain elevation: " + !!useTerrainElevation);

#ifdef CAGE_DEBUG
		constexpr bool navmeshOptimizeInit = false;
#else
		constexpr bool navmeshOptimizeInit = true;
#endif // CAGE_DEBUG
		ConfigBool navmeshOptimize("unnatural-planets/navmesh/optimize", navmeshOptimizeInit);
		navmeshOptimize = cmd->cmdBool('o', "optimize", navmeshOptimize);
		CAGE_LOG(SeverityEnum::Info, "configuration", stringizer() + "using navmesh optimizations: " + !!navmeshOptimize);
		
		ConfigBool saveDebugIntermediates("unnatural-planets/generator/saveIntermediateSteps", false);
		saveDebugIntermediates = cmd->cmdBool('i', "intermediate", saveDebugIntermediates);
	}
}

int main(int argc, const char *args[])
{
	try
	{
		Holder<Logger> log1 = newLogger();
		log1->format.bind<logFormatConsole>();
		log1->output.bind<logOutputStdOut>();

		{
			Holder<Ini> cmd = newIni();
			cmd->parseCmd(argc, args);
			applyConfiguration(cmd);
			cmd->checkUnused();
		}
		generateEntry();
		return 0;
	}
	catch (...)
	{
		detail::logCurrentCaughtException();
	}
	return 1;
}
