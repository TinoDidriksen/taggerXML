#ifndef TAGGER_H
#define TAGGER_H

#include "option.h"
#include "defines.h"
#include <iostream>
#include <string>

class tagger {
private:
public:
	tagger();
	bool init(
	  std::string_view optpath,
	  const std::string& Lexicon,
	  const std::string& Bigrams,
	  const std::string& Lexicalrulefile,
	  const std::string& Contextualrulefile,
	  const std::string& wdlistname,
	  bool START_ONLY_FLAG,
	  bool FINAL_ONLY_FLAG);
	bool analyse(std::istream& CORPUS, std::ostream& fpout, optionStruct* Options);
};


#endif
