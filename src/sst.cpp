/*  Copyright @ 1993 MIT and University of Pennsylvania*/
/* Written by Eric Brill */
/*THIS SOFTWARE IS PROVIDED "AS IS", AND M.I.T. MAKES NO REPRESENTATIONS 
OR WARRANTIES, EXPRESS OR IMPLIED.  By way of example, but not 
limitation, M.I.T. MAKES NO REPRESENTATIONS OR WARRANTIES OF 
MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR THAT THE USE OF 
THE LICENSED SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY 
PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.   */

/* Adapted by Bart Jongejan. (Conversion to Standard C and more.) */

#include "substring.h"
//#include "hashmap.h"
#include "utf8func.h"
#include "newregistry.h"
#include "lex.h"
#include "memory.h"
#include "useful.h"
#include "sst.h"
#include "staart.h"
#include "defines.h"
#include "option.h"
#include "bool.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdlib.h>
#include <ctype.h>
#include <locale.h>

inline int svtoi(std::string_view& str) {
	int rv = atoi(&str.front());
	return rv;
}

int CheckLineForCapitalizedWordsOrNonAlphabeticStuff(corpus* Corpus) {
	int tot = 0;
	int low = 0;
	for (int i = Corpus->startOfLine; i <= Corpus->endOfLine; ++i) {
		++tot;
		if (!isUpperUTF8(Corpus->Token[i].getWord())) {
			++low;
		}
	}
	return 2 * low < tot; /* return 1 if not heading (less than half of all words begin with lower case.)*/
}

int start_state_tagger(lmdb::env& lexicon_mdb, corpus* Corpus, lmdb::env& bigram_mdb, NewDarray& rule_array, lmdb::env& wordlist_mdb, optionStruct* Options, hashmap::hash<strng>* tag_hash) {
	int corpus_array_index = 0;
	hashmap::hash<strng>* ntot_hash; // for words (types) that are not in the lexicon.
	                                 //    hashmap::hash<strng> * tag_hash;
	strng** tag_array;
	std::string tempstr2;
	std::string_view noun{ "NN" };
	std::string_view proper{ "NNP" };
	int tempcount;
	unsigned int count, count2, count3;
	char tempstr_space[MAXWORDLEN + MAXAFFIXLEN];
	bool EXTRAWDS = (size(wordlist_mdb) != 0);
	long ConvertToLowerCaseIfFirstWord = option("ConvertToLowerCaseIfFirstWord");
	if (ConvertToLowerCaseIfFirstWord == -1) {
		ConvertToLowerCaseIfFirstWord = Options->ConvertToLowerCaseIfFirstWord ? 1 : 0;
	}

	long ConvertToLowerCaseIfMostWordsAreCapitalized = option("ConvertToLowerCaseIfMostWordsAreCapitalized");
	if (ConvertToLowerCaseIfMostWordsAreCapitalized == -1) {
		ConvertToLowerCaseIfMostWordsAreCapitalized = Options->ConvertToLowerCaseIfMostWordsAreCapitalized ? 1 : 0;
	}
	long Verbose = option("Verbose");
	if (Verbose == -1) {
		Verbose = Options->Verbose ? 1 : 0;
	}

	if (!Options->Noun.empty()) {
		noun = Options->Noun;
	}
	/*
	else {
		char* tmp;
		tmp = coption("Noun");
		if (tmp) {
			strncpy(noun, tmp, sizeof(noun) - 1);
			noun[sizeof(noun) - 1] = '\0';
		}
	}
	//*/

	if (!Options->Proper.empty()) {
		proper = Options->Proper;
	}
	/*
	else {
		char* tmp;
		tmp = coption("Proper");
		if (tmp) {
			strncpy(proper, tmp, sizeof(proper) - 1);
			proper[sizeof(proper) - 1] = '\0';
		}
	}
	//*/

	/***********************************************************************/


	/* Wordlist_hash contains a list of words.  This is used
        for tagging unknown words in the "add prefix/suffix" and
    "delete prefix/suffix" rules.  This contains words not in LEXICON. */

	/*********************************************************/
	/* Read in Corpus to be tagged.  Actually, just record word list, */
	/* since each word will get the same tag, regardless of context. */
	ntot_hash = new hashmap::hash<strng>(&strng::key, 1000);

	Corpus->rewind();

	INCREMENT
	INCREMENT

	lmdb_reader lex_rd(lexicon_mdb);
	lmdb_reader words_rd(wordlist_mdb);
	lmdb_reader bigram_rd(bigram_mdb);

	while (Corpus->getline()) {
		int startOfLine = Bool_TRUE;
		int heading = Bool_FALSE;
		if (ConvertToLowerCaseIfMostWordsAreCapitalized) {
			heading = CheckLineForCapitalizedWordsOrNonAlphabeticStuff(Corpus);
		}
		const char* wrd;
		while ((wrd = Corpus->getWord()) != NULL) {
			if (!exists(lex_rd, wrd) && ((heading && (!exists(lex_rd, allToLowerUTF8(wrd)))) || !startOfLine || ConvertToLowerCaseIfFirstWord != 1 || !exists(lex_rd, allToLowerUTF8(wrd)))) {
				void* bucket;
				if (!ntot_hash->find(wrd, bucket)) {
					ntot_hash->insert(new strng(wrd, NULL), bucket);
				}
			}
			startOfLine = Bool_FALSE;
		}
		substring::reset();
	}
	Corpus->rewind();

	if (Verbose) {
		fprintf(stderr, "START STATE TAGGER:: CORPUS READ\n");
	}

	size_t length;
	tag_array = ntot_hash->convertToList(length);
	delete ntot_hash;

	/********** START STATE ALGORITHM
    YOU CAN USE OR EDIT ONE OF THE TWO START STATE ALGORITHMS BELOW,
# OR REPLACE THEM WITH YOUR OWN ************************/

	/* UNCOMMENT THIS AND COMMENT OUT START STATE 2 IF ALL UNKNOWN WORDS
        SHOULD INITIALLY BE ASSUMED TO BE TAGGED WITH "NN".
    YOU CAN ALSO CHANGE "NN" TO A DIFFERENT TAG IF APPROPRIATE. */

	/*** START STATE 1 ***/
	/*   for (count= 0; count < Darray_len(tag_array_val);++count)
    Darray_set(tag_array_val,count,noun); */

	/* THIS START STATE ALGORITHM INITIALLY TAGS ALL UNKNOWN WORDS WITH TAG
    "NN" (singular common noun) UNLESS THEY BEGIN WITH A CAPITAL LETTER,
    IN WHICH CASE THEY ARE TAGGED WITH "NNP" (singular proper noun)
    YOU CAN CHANGE "NNP" and "NN" TO DIFFERENT TAGS IF APPROPRIATE.*/

	/*** START STATE 2 ***/
	for (count = 0; count < length /*Darray_len(tag_array_val)*/; ++count) {
		if (isUpperUTF8(tag_array[count]->key())) {
			tag_array[count]->setVal(proper);
		}
		else {
			tag_array[count]->setVal(noun);
		}
	}


	/******************* END START STATE ALGORITHM ****************/

	// Analyse the Out Of Vocabulary words with the lexical rules.
	size_t notokens = length;
	std::string tmp;

	for (size_t count = 0; count < rule_array.size(); ++count) {
		auto& therule = rule_array[count];
		if (Verbose) {
			fprintf(stderr, "s");
		}

		size_t rulesize = 0;
		for (auto& s : therule) {
			if (!s.empty()) {
				++rulesize;
			}
		}
		--rulesize;

		if (therule[1].compare("char") == 0) {
			for (count2 = 0; count2 < notokens; ++count2) {
				if (therule[rulesize - 1].compare(tag_array[count2]->val()) != 0) {
					if (therule[0].find_first_of(tag_array[count2]->key()) != std::string_view::npos) {
						tag_array[count2]->setVal(therule[rulesize - 1]);
					}
				}
			}
		}
		else if (therule[2].compare("fchar") == 0) {
			for (count2 = 0; count2 < notokens; ++count2) {
				if (therule[0].compare(tag_array[count2]->val()) == 0) {
					if (therule[1].find_first_of(tag_array[count2]->key()) != std::string_view::npos) {
						tag_array[count2]->setVal(therule[rulesize - 1]);
					}
				}
			}
		}
		else if (therule[1].compare("deletepref") == 0) {
			for (count2 = 0; count2 < notokens; ++count2) {
				if (therule[rulesize - 1].compare(tag_array[count2]->val()) != 0) {
					const char* tempstr = tag_array[count2]->key();
					for (count3 = 0; (int)count3 < svtoi(therule[2]); ++count3) {
						if (tempstr[count3] != therule[0][count3]) {
							break;
						}
					}
					if ((int)count3 == svtoi(therule[2])) {
						tempstr += svtoi(therule[2]);
						if (exists(lex_rd, (char*)tempstr) || (EXTRAWDS && exists(words_rd, (char*)tempstr))) {
							tag_array[count2]->setVal(therule[rulesize - 1]);
						}
					}
				}
			}
		}
		else if (therule[2].compare("fdeletepref") == 0) {
			for (count2 = 0; count2 < notokens; ++count2) {
				if (therule[0].compare(tag_array[count2]->val()) == 0) {
					const char* tempstr = tag_array[count2]->key();
					for (count3 = 0; (int)count3 < svtoi(therule[3]); ++count3) {
						if (tempstr[count3] != therule[1][count3]) {
							break;
						}
					}
					if ((int)count3 == svtoi(therule[3])) {
						tempstr += svtoi(therule[3]);
						if (exists(lex_rd, (char*)tempstr) || (EXTRAWDS && exists(words_rd, (char*)tempstr))) {
							tag_array[count2]->setVal(therule[rulesize - 1]);
						}
					}
				}
			}
		}
		else if (therule[1].compare("haspref") == 0) {
			for (count2 = 0; count2 < notokens; ++count2) {
				if (therule[rulesize - 1].compare(tag_array[count2]->val()) != 0) {
					const char* tempstr = tag_array[count2]->key();
					for (count3 = 0; (int)count3 < svtoi(therule[2]); ++count3) {
						if (tempstr[count3] != therule[0][count3]) {
							break;
						}
					}
					if ((int)count3 == svtoi(therule[2])) {
						tag_array[count2]->setVal(therule[rulesize - 1]);
					}
				}
			}
		}
		else if (therule[2].compare("fhaspref") == 0) {
			for (count2 = 0; count2 < notokens; ++count2) {
				if (therule[0].compare(tag_array[count2]->val()) == 0) {
					const char* tempstr = tag_array[count2]->key();
					for (count3 = 0; (int)count3 < svtoi(therule[3]); ++count3) {
						if (tempstr[count3] != therule[1][count3]) {
							break;
						}
					}
					if ((int)count3 == svtoi(therule[3])) {
						tag_array[count2]->setVal(therule[rulesize - 1]);
					}
				}
			}
		}
		else if (therule[1].compare("deletesuf") == 0) {
			for (count2 = 0; count2 < notokens; ++count2) {
				if (therule[rulesize - 1].compare(tag_array[count2]->val()) != 0) {
					const char* tempstr = tag_array[count2]->key();
					tempcount = strlen(tempstr) - svtoi(therule[2]);
					for (count3 = tempcount; count3 < strlen(tempstr); ++count3) {
						if (tempstr[count3] != therule[0][count3 - tempcount]) {
							break;
						}
					}
					if (count3 == strlen(tempstr)) {
						tempstr2.assign(tempstr, tempstr + tempcount);
						if (exists(lex_rd, tempstr2) || (EXTRAWDS && exists(words_rd, tempstr2))) {
							tag_array[count2]->setVal(therule[rulesize - 1]);
						}
						DECREMENT
					}
				}
			}
		}
		else if (therule[2].compare("fdeletesuf") == 0) {
			for (count2 = 0; count2 < notokens; ++count2) {
				if (therule[0].compare(tag_array[count2]->val()) == 0) {
					const char* tempstr = tag_array[count2]->key();
					tempcount = strlen(tempstr) - svtoi(therule[3]);
					for (count3 = tempcount; count3 < strlen(tempstr); ++count3) {
						if (tempstr[count3] != therule[1][count3 - tempcount]) {
							break;
						}
					}
					if (count3 == strlen(tempstr)) {
						tempstr2.assign(tempstr, tempstr + tempcount);
						if (exists(lex_rd, tempstr2) || (EXTRAWDS && exists(words_rd, tempstr2))) {
							tag_array[count2]->setVal(therule[rulesize - 1]);
						}
						DECREMENT
					}
				}
			}
		}
		else if (therule[1].compare("hassuf") == 0) {
			for (count2 = 0; count2 < notokens; ++count2) {
				if (therule[rulesize - 1].compare(tag_array[count2]->val()) != 0) {
					const char* tempstr = tag_array[count2]->key();
					tempcount = strlen(tempstr) - svtoi(therule[2]);
					for (count3 = tempcount; count3 < strlen(tempstr); ++count3) {
						if (tempstr[count3] != therule[0][count3 - tempcount]) {
							break;
						}
					}
					if (count3 == strlen(tempstr)) {
						tag_array[count2]->setVal(therule[rulesize - 1]);
					}
				}
			}
		}
		else if (therule[2].compare("fhassuf") == 0) {
			for (count2 = 0; count2 < notokens; ++count2) {
				if (therule[0].compare(tag_array[count2]->val()) == 0) {
					const char* tempstr = tag_array[count2]->key();
					tempcount = strlen(tempstr) - svtoi(therule[3]);
					for (count3 = tempcount; count3 < strlen(tempstr); ++count3) {
						if (tempstr[count3] != therule[1][count3 - tempcount]) {
							break;
						}
					}
					if (count3 == strlen(tempstr)) {
						tag_array[count2]->setVal(therule[rulesize - 1]);
					}
				}
			}
		}
		else if (therule[1].compare("addpref") == 0) {
			for (count2 = 0; count2 < notokens; ++count2) {
				if (therule[rulesize - 1].compare(tag_array[count2]->val()) == 0) {
					tempstr_space[0] = 0;
					strncat(tempstr_space, &therule[0].front(), therule[0].size());
					strcat(tempstr_space, tag_array[count2]->key());
					if (exists(lex_rd, (char*)tempstr_space) || (EXTRAWDS && exists(words_rd, (char*)tempstr_space))) {
						tag_array[count2]->setVal(therule[rulesize - 1]);
					}
				}
			}
		}
		else if (therule[2].compare("faddpref") == 0) {
			for (count2 = 0; count2 < notokens; ++count2) {
				if (therule[rulesize - 1].compare(tag_array[count2]->val()) == 0) {
					tempstr_space[0] = 0;
					strncat(tempstr_space, &therule[1].front(), therule[1].size());
					strcat(tempstr_space, tag_array[count2]->key());
					if (exists(lex_rd, (char*)tempstr_space) || (EXTRAWDS && exists(words_rd, (char*)tempstr_space))) {
						tag_array[count2]->setVal(therule[rulesize - 1]);
					}
				}
			}
		}
		else if (therule[1].compare("addsuf") == 0) {
			for (count2 = 0; count2 < notokens; ++count2) {
				if (therule[rulesize - 1].compare(tag_array[count2]->val()) != 0) {
					tempstr_space[0] = 0;
					strcat(tempstr_space, tag_array[count2]->key());
					strncat(tempstr_space, &therule[0].front(), therule[0].size());
					if (exists(lex_rd, (char*)tempstr_space) || (EXTRAWDS && exists(words_rd, (char*)tempstr_space))) {
						tag_array[count2]->setVal(therule[rulesize - 1]);
					}
				}
			}
		}
		else if (therule[2].compare("faddsuf") == 0) {
			for (count2 = 0; count2 < notokens; ++count2) {
				if (therule[0].compare(tag_array[count2]->val()) == 0) {
					tempstr_space[0] = 0;
					strcat(tempstr_space, tag_array[count2]->key());
					strncat(tempstr_space, &therule[1].front(), therule[1].size());
					if (exists(lex_rd, (char*)tempstr_space) || (EXTRAWDS && exists(words_rd, (char*)tempstr_space))) {
						tag_array[count2]->setVal(therule[rulesize - 1]);
					}
				}
			}
		}
		else if (therule[1].compare("goodleft") == 0) {
			for (count2 = 0; count2 < notokens; ++count2) {
				if (therule[rulesize - 1].compare(tag_array[count2]->val()) != 0) {
					join(tmp, tag_array[count2]->key(), '\t', therule[0]);
					if (exists(bigram_rd, tmp)) {
						tag_array[count2]->setVal(therule[rulesize - 1]);
					}
				}
			}
		}
		else if (therule[2].compare("fgoodleft") == 0) {
			for (count2 = 0; count2 < notokens; ++count2) {
				if (therule[0].compare(tag_array[count2]->val()) == 0) {
					join(tmp, tag_array[count2]->key(), '\t', therule[1]);
					if (exists(bigram_rd, tmp)) {
						tag_array[count2]->setVal(therule[rulesize - 1]);
					}
				}
			}
		}
		else if (therule[1].compare("goodright") == 0) {
			for (count2 = 0; count2 < notokens; ++count2) {
				if (therule[rulesize - 1].compare(tag_array[count2]->val()) != 0) {
					join(tmp, therule[0], '\t', tag_array[count2]->key());
					if (exists(bigram_rd, tmp)) {
						tag_array[count2]->setVal(therule[rulesize - 1]);
					}
				}
			}
		}
		else if (therule[2].compare("fgoodright") == 0) {
			for (count2 = 0; count2 < notokens; ++count2) {
				if (therule[0].compare(tag_array[count2]->val()) == 0) {
					join(tmp, therule[1], '\t', tag_array[count2]->key());
					if (exists(bigram_rd, tmp)) {
						tag_array[count2]->setVal(therule[rulesize - 1]);
					}
				}
			}
		}
	}
	if (Verbose) {
		fprintf(stderr, "\n");
	}


	/* now go from array to hash table */

	//    tag_hash = new hashmap::hash<strng>(&strng::key,1000);
	for (count = 0; count < length; ++count) {
		auto name = tag_array[count]->key();
		auto val = tag_array[count]->val();
		void* bucket;
		if (!tag_hash->find(name, bucket)) {
			tag_hash->insert(new strng(name, val), bucket);
		}
	}

	for (size_t i = 0; i < length; i++) {
		delete tag_array[i];
	}
	delete[] tag_array;

	while (Corpus->getline()) {
		int startOfLine = Bool_TRUE;
		int heading = Bool_FALSE;
		if (ConvertToLowerCaseIfMostWordsAreCapitalized) {
			heading = CheckLineForCapitalizedWordsOrNonAlphabeticStuff(Corpus);
		}

		token* Tok;
		const char* wrd;
		for (int i = Corpus->startOfLine; i <= Corpus->endOfLine; ++i) {
			Tok = Corpus->Token + i;
			wrd = Tok->getWord();
			std::string_view tempstr;

			if (Tok->PreTag) {
				Tok->Pos = Tok->PreTag; // assume tag from input
			}
			else if (!(tempstr = find_or_default(lex_rd, wrd)).empty() || ((heading || (startOfLine && ConvertToLowerCaseIfFirstWord == 1)) && !(tempstr = find_or_default(lex_rd, allToLowerUTF8(wrd))).empty())) {
				Tok->Pos = tempstr; // assume tag from lexicon
			}
			else {
				void* bucket;
				strng* found = tag_hash->find(wrd, bucket);
				assert(found);
				Tok->Pos = found->val(); // assume tag from morphological analysis
			}
			startOfLine = Bool_FALSE;
		}
		substring::reset();
	}
	Corpus->rewind();
	/*
    tag_hash->deleteThings();
    delete tag_hash;
*/
	return corpus_array_index;
}
