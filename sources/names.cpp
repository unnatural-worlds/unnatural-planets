#include "planets.h"

#include <cage-core/math.h>
#include <cage-core/string.h>

namespace unnatural
{
	namespace
	{
		constexpr const char *Prefixes[] = {
			"aq",
			"bel",
			"bell",
			"bich",
			"byw",
			"cit",
			"civ",
			"dehr",
			"ev",
			"fur",
			"han",
			"jux",
			"kol",
			"lip",
			"lis",
			"lok",
			"mar",
			"mus",
			"nar",
			"natr",
			"nis",
			"pos",
			"rap",
			"res",
			"sag",
			"saof",
			"ss",
			"wep",
			"xan",
			"zug",
		};
		constexpr const char *Stems[] = {
			"a",
			"ab",
			"abe",
			"ad",
			"adur",
			"aes",
			"al",
			"ali",
			"amo",
			"amora",
			"anim",
			"ante",
			"ape",
			"apoll",
			"aster",
			"atque",
			"e",
			"educ",
			"eius",
			"emo",
			"empu",
			"equi",
			"equis",
			"erbe",
			"erra",
			"erti",
			"et",
			"eu",
			"ex",
			"extr",
			"guius",
			"hann",
			"hum",
			"i",
			"iace",
			"ibu",
			"igre",
			"ijka",
			"ille",
			"illud",
			"imac",
			"imnu",
			"imu",
			"inept",
			"iris",
			"iste",
			"ita",
			"iuv",
			"jim",
			"o",
			"obe",
			"ocul",
			"ofe",
			"oi",
			"orbis",
			"ordi",
			"osse",
			"u",
			"ud",
			"udire",
			"uli",
			"urb",
			"ursu",
			"usu",
			"uzi",
			"y",
			"yre",
		};
		constexpr const char *Suffixes[] = {
			"ais",
			"at",
			"ath",
			"ator",
			"axia",
			"eka",
			"el",
			"emus",
			"er",
			"es",
			"imus",
			"ith",
			"itur",
			"ix",
			"o",
			"or",
			"orex",
			"ox",
			"uh",
			"ul",
			"um",
			"us",
			"y",
			"yi",
		};
		constexpr const char *Appendixes[] = {
			"I",
			"II",
			"III",
			"IV",
			"V",
			"VI",
			"VII",
			"VIII",
			"IX",
			"X",
		};
#define PICK(NAMES) NAMES[randomRange(std::size_t(0), array_size(NAMES))]

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
			if (randomChance() < 0.25)
				name + " " + PICK(Appendixes);
			if (randomChance() < 0.15)
				name + "-" + PICK(Appendixes);
			return name;
		}

#if 0
	struct NamesPrinter
	{
		NamesPrinter()
		{
			for (uint32 i = 0; i < 100; i++)
				CAGE_LOG(SeverityEnum::Info, "NamesPrinter", generateNameImpl());
			CAGE_THROW_CRITICAL(Exception, "names printed, stopping the execution now");
		}
	} namesPrinter;
#endif
	}

	String generateName()
	{
		String name = generateNameImpl();
		name[0] = toUpper(String(name[0]))[0];
		return name;
	}
}
