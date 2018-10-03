#ifndef WRITECORPUS_H
#define WRITECORPUS_H

#include "defines.h"
#include <iostream>

void writeCorpus(int corpus_max_index, char** word_corpus_array, char** tag_corpus_array, std::ostream& fpout);

#endif
