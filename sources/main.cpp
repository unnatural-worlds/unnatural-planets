#include "generator.h"

#include <cage-core/logger.h>

#include <exception>

int main(int argc, const char *args[])
{
	try
	{
		Holder<Logger> log1 = newLogger();
		log1->format.bind<logFormatConsole>();
		log1->output.bind<logOutputStdOut>();

		generateEntry();
		return 0;
	}
	catch (const cage::Exception &e)
	{
		CAGE_LOG(SeverityEnum::Note, "exception", e.message);
		CAGE_LOG(SeverityEnum::Error, "exception", "caught cage exception in main");
	}
	catch (const std::exception &e)
	{
		CAGE_LOG(SeverityEnum::Note, "exception", e.what());
		CAGE_LOG(SeverityEnum::Error, "exception", "caught std exception in main");
	}
	catch (...)
	{
		CAGE_LOG(SeverityEnum::Error, "exception", "caught unknown exception in main");
	}
	return 1;
}
