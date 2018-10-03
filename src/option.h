#ifndef OPTION_H
#define OPTION_H

#include "defines.h"
#include <string>
#include <string_view>

typedef enum {
	GoOn = 0,
	Leave = 1,
	Error = 2,
} OptReturnTp;

struct optionStruct {
	std::string OptionPath;
	std::string Lexicon;
	std::string Corpus;
	std::string Bigrams;
	std::string Lexicalrulefile;
	std::string Contextualrulefile;
	std::string wdlistname;
	bool START_ONLY_FLAG = false;
	bool FINAL_ONLY_FLAG = false;
	std::string xtra; // default: xoptions file (deprecated)

	bool ConvertToLowerCaseIfFirstWord = false;               // -f
	bool ConvertToLowerCaseIfMostWordsAreCapitalized = false; // -a
	bool ShowIfLowercaseConversionHelped = false;             // -s
	std::string Noun;                                         // -n
	std::string Proper;                                       // -p
	bool Verbose = false;                                     // -v

	bool XML = false;
	std::string ancestor;
	std::string segment;
	std::string element;
	std::string wordAttribute;
	std::string PreTagAttribute;
	std::string POSAttribute;
	std::string Output;

	OptReturnTp doSwitch(int c, std::string_view locoptarg, std::string_view progname);
	OptReturnTp readOptsFromFile(std::string_view locoptarg, std::string_view progname);
	OptReturnTp readArgs(int argc, char* argv[]);
};

#endif
