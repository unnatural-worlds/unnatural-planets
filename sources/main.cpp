#include "generator.h"

#include <cage-core/config.h>
#include <cage-core/ini.h>
#include <cage-core/logger.h>
#include <cage-core/string.h>

namespace unnatural
{
	void terrainApplyConfig();
	extern String configOverrideOutputPath;

	namespace
	{
		void applyConfiguration(const Holder<Ini>& cmd)
		{
			ConfigString configShapeMode("unnatural-planets/shape/mode", "random");
			configShapeMode = cmd->cmdString('s', "shape", configShapeMode);
			configShapeMode = toLower((String)configShapeMode);

			ConfigString configElevationMode("unnatural-planets/elevation/mode", "lakes");
			configElevationMode = cmd->cmdString('e', "elevation", configElevationMode);
			configElevationMode = toLower((String)configElevationMode);

			ConfigString configColoringMode("unnatural-planets/coloring/mode", "default");
			configColoringMode = cmd->cmdString('c', "colors", configColoringMode);
			configColoringMode = toLower((String)configColoringMode);

			ConfigBool configPolesEnable("unnatural-planets/poles/enable", false);
			configPolesEnable = cmd->cmdBool('p', "poles", configPolesEnable);

			ConfigBool configFlowersEnable("unnatural-planets/flowers/enable", false);
			configFlowersEnable = cmd->cmdBool('f', "flowers", configFlowersEnable);

			terrainApplyConfig();

			ConfigBool configNavmeshOptimize("unnatural-planets/navmesh/optimize", !CAGE_DEBUG_BOOL);
			configNavmeshOptimize = cmd->cmdBool('o', "optimize", configNavmeshOptimize);
			CAGE_LOG(SeverityEnum::Info, "configuration", Stringizer() + "enable navmesh optimizations: " + !!configNavmeshOptimize);

			ConfigBool configDebugSaveIntermediate("unnatural-planets/debug/saveIntermediate", false);
			configDebugSaveIntermediate = cmd->cmdBool('d', "debug", configDebugSaveIntermediate);
			CAGE_LOG(SeverityEnum::Info, "configuration", Stringizer() + "enable saving intermediates for debug: " + !!configDebugSaveIntermediate);

			ConfigBool configPreviewEnable("unnatural-planets/preview/enable", false);
			configPreviewEnable = cmd->cmdBool('r', "preview", configPreviewEnable);
			CAGE_LOG(SeverityEnum::Info, "configuration", Stringizer() + "enable preview: " + !!configPreviewEnable);

			// configOverrideOutputPath should not be stored in the ini file
			configOverrideOutputPath = cmd->cmdString('v', "outputPathOverride", "");
			CAGE_LOG(SeverityEnum::Info, "configuration", Stringizer() + "override output path: " + configOverrideOutputPath);
		}
	}
}

int main(int argc, const char* args[])
{
	using namespace unnatural;

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
