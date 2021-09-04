#include <cage-core/math.h>
#include <cage-core/string.h>

using namespace cage;

namespace
{
	constexpr const char *Prefixes[] = {
		"bel", "nar", "xan", "bell", "natr", "ev",
	};
	constexpr const char *Stems[] = {
		"adur", "aes", "anim", "apoll", "imac",
		"educ", "equis", "extr", "guius", "hann",
		"equi", "amora", "hum", "iace", "ille",
		"inept", "iuv", "obe", "ocul", "orbis",
	};
	constexpr const char *Suffixes[] = {
		"us", "ix", "ox", "ith", "ath", "um",
		"ator", "or", "axia", "imus", "ais",
		"itur", "orex", "o", "y",
	};
	constexpr const char *Appendixes[] = {
		" I", " II", " III", " IV", " V",
		" VI", " VII", " VIII", " IX", " X",
	};
#define PICK(NAMES) NAMES[randomRange(std::size_t(0), sizeof(NAMES)/sizeof(NAMES[0]))]

	String generateNameImpl()
	{
		Stringizer name;
		if (randomChance() < 0.5)
			name + PICK(Prefixes);
		if (randomChance() < 0.8)
			name + PICK(Stems);
		if (randomChance() < 0.1)
			name + PICK(Stems);
		if (randomChance() < 0.5)
			name + PICK(Suffixes);
		if (String(name).length() < 3)
			return generateNameImpl();
		if (randomChance() < 0.1)
			name = Stringizer() + reverse(String(name));
		if (randomChance() < 0.4)
			name + PICK(Appendixes);
		return name;
	}
}

String generateName()
{
	String name = generateNameImpl();
	name[0] = toUpper(String(name[0]))[0];
	return name;
}
