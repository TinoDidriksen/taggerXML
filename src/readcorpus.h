#ifndef READCORPUS_H
#define READCORPUS_H

#include "defines.h"
#include <iostream>

int readcorpus(std::istream& CORPUS, char*** WORD_CORPUS_ARRAY, char*** TAG_CORPUS_ARRAY, int corpussize);

#endif
