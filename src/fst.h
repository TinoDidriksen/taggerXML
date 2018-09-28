#ifndef FST_H
#define FST_H

#include "XMLtext.h"
#include "registry.h"
#include "defines.h"
#include <iostream>

struct optionStruct;

int final_state_tagger(  const char * Contextualrulefile
#if RESTRICT_MOVE
#if WITHSEENTAGGING
                       , Registry SEENTAGGING
#endif
                       , NewRegistry& WORDS
#endif
                       , text * Corpus
                       , optionStruct * Options
                       , std::ostream & fpout
                       );

#endif
