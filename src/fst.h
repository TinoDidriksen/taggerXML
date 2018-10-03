#ifndef FST_H
#define FST_H

#include "XMLtext.h"
#include "newregistry.h"
#include "defines.h"
#include <iostream>
#include <string>
#include <string_view>

struct optionStruct;

int final_state_tagger(std::string Contextualrulefile,
#if RESTRICT_MOVE
#	if WITHSEENTAGGING
  Registry SEENTAGGING,
#	endif
  NewRegistry& WORDS,
#endif
  text* Corpus, optionStruct* Options, std::ostream& fpout);

#endif
