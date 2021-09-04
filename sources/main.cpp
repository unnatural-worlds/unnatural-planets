#include <cage-core/logger.h>
#include <cage-core/ini.h>
#include <cage-core/config.h>
#include <cage-core/string.h>

#include "terrain.h"
#include "generator.h"

namespace
{
	void applyConfiguration(const Holder<Ini> &cmd)
	{
		ConfigString configShapeMode("unnatural-planets/shape/mode", "random");
		configShapeMode = cmd->cmdString('s', "shape", configShapeMode);
		configShapeMode = toLower((String)configShapeMode);

		ConfigString configElevationMode("unnatural-planets/elevation/mode", "lakes");
		configElevationMode = cmd->cmdString('e', "elevation", configElevationMode);
		configElevationMode = toLower((String)configElevationMode);

		terrainApplyConfig();

		ConfigBool configPolesEnable("unnatural-planets/poles/enable", (String)configShapeMode == "sphere");
		configPolesEnable = cmd->cmdBool('p', "poles", configPolesEnable);
		CAGE_LOG(SeverityEnum::Info, "configuration", Stringizer() + "enable poles: " + !!configPolesEnable);

#ifdef CAGE_DEBUG
		constexpr bool navmeshOptimizeInit = false;
#else
		constexpr bool navmeshOptimizeInit = true;
#endif // CAGE_DEBUG
		ConfigBool configNavmeshOptimize("unnatural-planets/navmesh/optimize", navmeshOptimizeInit);
		configNavmeshOptimize = cmd->cmdBool('o', "optimize", configNavmeshOptimize);
		CAGE_LOG(SeverityEnum::Info, "configuration", Stringizer() + "enable navmesh optimizations: " + !!configNavmeshOptimize);
		
		ConfigBool configDebugSaveIntermediate("unnatural-planets/debug/saveIntermediate", false);
		configDebugSaveIntermediate = cmd->cmdBool('d', "debug", configDebugSaveIntermediate);
		CAGE_LOG(SeverityEnum::Info, "configuration", Stringizer() + "enable saving intermediates for debug: " + !!configDebugSaveIntermediate);

		ConfigBool configPreviewEnable("unnatural-planets/preview/enable", false);
		configPreviewEnable = cmd->cmdBool('r', "preview", configPreviewEnable);
		CAGE_LOG(SeverityEnum::Info, "configuration", Stringizer() + "enable preview: " + !!configPreviewEnable);
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
			cmd->checkUnusedWithHelp();
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
