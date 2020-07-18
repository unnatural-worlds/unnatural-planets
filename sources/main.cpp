#include "generator.h"

#include <cage-core/logger.h>
#include <cage-core/ini.h>

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
			functionsConfigure(cmd);
			meshConfigure(cmd);
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
