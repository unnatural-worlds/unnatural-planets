#include <exception>
#include "generator.h"

int main(int argc, const char *args[])
{
	try
	{
		holder<loggerClass> log1 = newLogger();
		log1->filter.bind<logFilterPolicyPass>();
		log1->format.bind<logFormatPolicyConsole>();
		log1->output.bind<logOutputPolicyStdOut>();

		if (argc != 2)
			CAGE_THROW_ERROR(exception, "the generator needs one argument - the random generator seed");

		generator::globalSeed = string(args[1]).toUint64();
		CAGE_LOG(severityEnum::Info, "generator", string() + "using seed " + generator::globalSeed);
		generator::generateMap();

		return 0;
	}
	catch (const cage::exception &e)
	{
		CAGE_LOG(severityEnum::Note, "exception", e.message);
		CAGE_LOG(severityEnum::Error, "grid", "caught cage exception in main");
	}
	catch (const std::exception &e)
	{
		CAGE_LOG(severityEnum::Note, "exception", e.what());
		CAGE_LOG(severityEnum::Error, "grid", "caught std exception in main");
	}
	catch (...)
	{
		CAGE_LOG(severityEnum::Error, "grid", "caught unknown exception in main");
	}
	return 1;
}
