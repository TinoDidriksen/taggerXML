#ifndef TAGGER_H
#define TAGGER_H

#include "option.h"
#include "defines.h"
#include <iostream>
#include <malloc.h>

/*
#if defined __MSDOS__ || defined _WIN32			//if PC version

#define START_PROG "start-state-tagger.exe"
#define END_PROG "final-state-tagger.exe"

#else  

#define START_PROG "./start-state-tagger" 
#define END_PROG "./final-state-tagger"

#endif
*/

class tagger {
private:
public:
	tagger();
	bool init(
	  const char* Lexicon,
	  const char* Bigrams,
	  const char* Lexicalrulefile,
	  const char* Contextualrulefile,
	  //char *intermed,
	  const char* wdlistname,
	  bool START_ONLY_FLAG,
	  bool FINAL_ONLY_FLAG);
	bool analyse(std::istream& CORPUS, /*std::ostream * fintermed,*/ std::ostream& fpout, optionStruct* Options);
};


#endif
