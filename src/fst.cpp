/* Copyright @ 1993 MIT and University of Pennsylvania */
/* Written by Eric Brill */
/*THIS SOFTWARE IS PROVIDED "AS IS", AND M.I.T. MAKES NO REPRESENTATIONS 
OR WARRANTIES, EXPRESS OR IMPLIED.  By way of example, but not 
limitation, M.I.T. MAKES NO REPRESENTATIONS OR WARRANTIES OF 
MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR THAT THE USE OF 
THE LICENSED SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY 
PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.   */

#include "lex.h"
#include "memory.h"
#include "useful.h"
#include "fst.h"
#include "option.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

inline bool cmppos(std::string_view pos, std::string_view p) {
	// This is not quite like .find() != npos because it must only match space-delimited substrings
	while (!p.empty()) {
		auto t = pos;
		while (!t.empty() && t.front() == p.front()) {
			t.remove_prefix(1);
			p.remove_prefix(1);
		}
		if (t.empty() && (p.empty() || p.front() == ' ')) {
			return true;
		}
		while (!p.empty() && p.front() != ' ') {
			p.remove_prefix(1);
		}
		if (!p.empty() && p.front() == ' ') {
			p.remove_prefix(1);
		}
	}
	return false;
}

bool sametokpos(std::string_view pos, token* tok) {
	auto p = tok->Pos;
	if (!tok->Pos.empty()) {
		while (!pos.empty() && pos.front() == p.front()) {
			pos.remove_prefix(1);
			p.remove_prefix(1);
		}
	}
	return pos.empty() && (p.empty() || p.front() == ' ' /* || p.front() == '\t'*/);
}

char* shift(char* line) {
	while (*line) {
		if (0 < *line && *line <= ' ') {
			*line = 0;
			return line + 1;
		}
		++line;
	}
	return NULL;
}

auto shift(std::string_view& line) {
	for (size_t i = 0; i < line.size(); ++i) {
		if (line[i] == ' ') {
			auto rv = line.substr(0, i);
			line.remove_prefix(i + 1);
			return rv;
		}
	}
	return line;
}

int final_state_tagger(std::string Contextualrulefile,
#if RESTRICT_MOVE
#	if WITHSEENTAGGING
  Registry SEENTAGGING,
#	endif
  lmdb::env& WORDS,
#endif
  text* Corpus, optionStruct* Options, std::ostream& fpout) {
	if (Corpus->numberOfTokens <= 0) {
		return 1;
	}

	int corpus_max_index;
	int count;
	const char* curwd;
	std::string_view tag, lft, rght, prev1, prev2, next1, next2, word;
	long Verbose = option("Verbose");
	if (Verbose == -1) {
		Verbose = Options->Verbose ? 1 : 0;
	}

	if (Verbose) { /* Bart 20030224 */
		fprintf(stderr, "FINAL STATE TAGGER:: READ IN OUTPUT FROM START\n");
	}

	if (Verbose) { /* Bart 20030224*/
		fprintf(stderr, "FINAL STATE TAGGER:: READ IN LEXICON\n");
	}

	lmdb_reader lex_rd(WORDS);

	/* read in rule file, and process each rule */
	Contextualrulefile = find_file(Options->OptionPath, Contextualrulefile);
	mmap_region changefile_mmap{ Contextualrulefile };
	corpus_max_index = Corpus->numberOfTokens;
	int linecnt = 0;
	for (std::string_view line{ nextline(changefile_mmap) }; !line.empty(); line = nextline(changefile_mmap, line)) {
		++linecnt;
		auto old = shift(line);
		auto neww = shift(line);
		auto when = shift(line);

		if (Verbose) { // Bart 20030224
			fprintf(stderr, "OLD: %s NEW: %s WHEN: %s\n", old.data(), neww.data(), when.data());
		}
		if (strcmp(when, "NEXTTAG") == 0 ||
		    strcmp(when, "NEXT2TAG") == 0 ||
		    strcmp(when, "NEXT1OR2TAG") == 0 ||
		    strcmp(when, "NEXT1OR2OR3TAG") == 0 ||
		    strcmp(when, "PREVTAG") == 0 ||
		    strcmp(when, "PREV2TAG") == 0 ||
		    strcmp(when, "PREV1OR2TAG") == 0 ||
		    strcmp(when, "PREV1OR2OR3TAG") == 0) {
			tag = shift(line);
		}
		else if (strcmp(when, "NEXTWD") == 0 ||
		         strcmp(when, "CURWD") == 0 ||
		         strcmp(when, "NEXT2WD") == 0 ||
		         strcmp(when, "NEXT1OR2WD") == 0 ||
		         strcmp(when, "NEXT1OR2OR3WD") == 0 ||
		         strcmp(when, "PREVWD") == 0 ||
		         strcmp(when, "PREV2WD") == 0 ||
		         strcmp(when, "PREV1OR2WD") == 0 ||
		         strcmp(when, "PREV1OR2OR3WD") == 0) {
			word = shift(line);
		}
		else if (strcmp(when, "SURROUNDTAG") == 0) {
			lft = shift(line);
			rght = shift(line);
		}
		else if (strcmp(when, "PREVBIGRAM") == 0) {
			prev1 = shift(line);
			prev2 = shift(line);
		}
		else if (strcmp(when, "NEXTBIGRAM") == 0) {
			next1 = shift(line);
			next2 = shift(line);
		}
		else if (strcmp(when, "LBIGRAM") == 0 ||
		         strcmp(when, "WDPREVTAG") == 0) {
			prev1 = shift(line);
			word = shift(line);
		}
		else if (strcmp(when, "RBIGRAM") == 0 ||
		         strcmp(when, "WDNEXTTAG") == 0) {
			word = shift(line);
			next1 = shift(line);
		}
		else if (strcmp(when, "WDAND2BFR") == 0 ||
		         strcmp(when, "WDAND2TAGBFR") == 0) {
			prev2 = shift(line);
			word = shift(line);
		}
		else if (strcmp(when, "WDAND2AFT") == 0 ||
		         strcmp(when, "WDAND2TAGAFT") == 0) {
			next2 = shift(line);
			word = shift(line);
		}
		for (count = 0; count < corpus_max_index; ++count) {
			if (sametokpos(old, Corpus->Token + count)) {
				curwd = Corpus->Token[count].getWord();
#if RESTRICT_MOVE
#	if WITHSEENTAGGING
				sprintf(atempstr2, "%s %s", curwd, neww);
				if (!Registry_get(WORDS, curwd)             // WORDS contains words, no tags
				    || Registry_get(SEENTAGGING, atempstr2) // SEENTAGGING contains word tag pairs
				)
#	else
				auto tagsToMatch = find_or_default(lex_rd, curwd); // WORDS (the Lexicon) contains word tag tag tag ... sequences
				/*
                    while(tagsToMatch)
                        {
                        char * endOfTag = strchr(tagsToMatch,' ');
                        if(endOfTag)
                            {
                            *endOfTag = '\0';
                            if(!strcmp(tagsToMatch,neww))
                                tagsToMatch = NULL;
                            else
                                tagsToMatch = endOfTag +1;
                            *endOfTag = ' ';
                            }
                        else
                            {
                            if(!strcmp(tagsToMatch,neww))
                                tagsToMatch = NULL;
                            break;
                            }
                        }
                    if(!tagsToMatch) // Apply contextual rule if the new tag
                        // is one of the allowed tags according to the lexicon,
                        // or if the lexicon does not contain the word.
                        */
				if (tagsToMatch.empty() || cmppos(neww, tagsToMatch))

#	endif
#endif
				{
					if (strcmp(when, "SURROUNDTAG") == 0) {
						if (count < corpus_max_index && count > 0) {
							/*
                                char * a = Corpus->prev1(count)->Pos;
                                char * b = Corpus->next1(count)->Pos;
                                if(!a)
                                    a = Corpus->prev1(count)->Pos;
                                assert(a);
                                assert(b);
                                assert(lft);
                                assert(rght);
                                */
							if (sametokpos(lft, Corpus->prev1(count) /*tag_corpus_array[count-1]*/) &&
							    sametokpos(rght, Corpus->next1(count) /*tag_corpus_array[count+1]*/)) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "NEXTTAG") == 0) {
						if (count < corpus_max_index) {
							if (sametokpos(tag, Corpus->next1(count) /*tag_corpus_array[count+1]*/)) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "CURWD") == 0) {
						if (strcmp_nocase(word, Corpus->Token[count].getWord() /*word_corpus_array[count]*/) == 0) {
							Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
						}
					}
					else if (strcmp(when, "NEXTWD") == 0) {
						if (count < corpus_max_index) {
							if (strcmp_nocase(word, Corpus->next1(count)->getWord() /*word_corpus_array[count+1]*/) == 0) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "RBIGRAM") == 0) {
						if (count < corpus_max_index) {
							if (strcmp_nocase(word, Corpus->Token[count].getWord() /*word_corpus_array[count]*/) ==
							      0 &&
							    strcmp_nocase(next1, Corpus->next1(count)->getWord() /*word_corpus_array[count+1]*/) ==
							      0) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "WDNEXTTAG") == 0) {
						if (count < corpus_max_index) {
							if (strcmp_nocase(word, Corpus->Token[count].getWord() /*word_corpus_array[count]*/) ==
							      0 &&
							    sametokpos(next1, Corpus->next1(count) /*tag_corpus_array[count+1]*/)) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "WDAND2AFT") == 0) {
						if (count < corpus_max_index - 1) {
							if (strcmp_nocase(word, Corpus->Token[count].getWord() /*word_corpus_array[count]*/) ==
							      0 &&
							    strcmp_nocase(next2, Corpus->next2(count)->getWord() /*word_corpus_array[count+2]*/) ==
							      0) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "WDAND2TAGAFT") == 0) {
						if (count < corpus_max_index - 1) {
							if (strcmp_nocase(word, Corpus->Token[count].getWord() /*word_corpus_array[count]*/) ==
							      0 &&
							    sametokpos(next2, Corpus->next2(count) /*tag_corpus_array[count+2]*/)) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "NEXT2TAG") == 0) {
						if (count < corpus_max_index - 1) {
							if (sametokpos(tag, Corpus->next2(count) /*tag_corpus_array[count+2]*/)) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "NEXT2WD") == 0) {
						if (count < corpus_max_index - 1) {
							if (strcmp_nocase(word, Corpus->next2(count)->getWord() /*word_corpus_array[count+2]*/) == 0) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "NEXTBIGRAM") == 0) {
						if (count < corpus_max_index - 1) {
							if (sametokpos(next1, Corpus->next1(count) /*tag_corpus_array[count+1]*/) &&
							    sametokpos(next2, Corpus->next2(count) /*tag_corpus_array[count+2]*/)) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "NEXT1OR2TAG") == 0) {
						if (count < corpus_max_index) {
							if (sametokpos(tag, Corpus->next1(count) /*tag_corpus_array[count+1]*/) ||
							    sametokpos(tag, Corpus->next1(count) /*tag_corpus_array[tempcount1]*/)) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "NEXT1OR2WD") == 0) {
						if (count < corpus_max_index) {
							if (strcmp_nocase(word, Corpus->next1(count)->getWord() /*word_corpus_array[count+1]*/) == 0 ||
							    strcmp_nocase(word, Corpus->next2(count)->getWord() /*word_corpus_array[tempcount1]*/) == 0) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "NEXT1OR2OR3TAG") == 0) {
						if (count < corpus_max_index) {
							if (sametokpos(tag, Corpus->next1(count) /*tag_corpus_array[count+1]*/) ||
							    sametokpos(tag, Corpus->nextn(2, count) /*tag_corpus_array[tempcount1]*/) ||
							    sametokpos(tag, Corpus->nextn(3, count) /*tag_corpus_array[tempcount2]*/)) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "NEXT1OR2OR3WD") == 0) {
						if (count < corpus_max_index) {
							if (strcmp_nocase(word, Corpus->next1(count)->getWord() /*word_corpus_array[count+1]*/) == 0 ||
							    strcmp_nocase(word, Corpus->nextn(2, count)->getWord() /*word_corpus_array[tempcount1]*/) == 0 ||
							    strcmp_nocase(word, Corpus->nextn(3, count)->getWord() /*word_corpus_array[tempcount2]*/) == 0) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "PREVTAG") == 0) {
						if (count > 0) {
							if (sametokpos(tag, Corpus->prev1(count) /*tag_corpus_array[count-1]*/)) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "PREVWD") == 0) {
						if (count > 0) {
							if (0 == strcmp_nocase(word, Corpus->prev1(count)->getWord() /*word_corpus_array[count-1]*/)) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "LBIGRAM") == 0) {
						if (count > 0) {
							if (0 == strcmp_nocase(word, Corpus->Token[count].getWord() /*word_corpus_array[count]*/) &&
							    0 == strcmp_nocase(prev1, Corpus->prev1(count)->getWord() /*word_corpus_array[count-1]*/)) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "WDPREVTAG") == 0) {
						if (count > 0) {
							if (0 == strcmp_nocase(word, Corpus->Token[count].getWord() /*word_corpus_array[count]*/) &&
							    sametokpos(prev1, Corpus->prev1(count) /*tag_corpus_array[count-1]*/)) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "WDAND2BFR") == 0) {
						if (count > 1) {
							if (0 == strcmp_nocase(word, Corpus->Token[count].getWord() /*word_corpus_array[count]*/) &&
							    0 == strcmp_nocase(prev2, Corpus->prev2(count)->getWord() /*word_corpus_array[count-2]*/)) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "WDAND2TAGBFR") == 0) {
						if (count > 1) {
							if (0 == strcmp_nocase(word, Corpus->Token[count].getWord() /*word_corpus_array[count]*/) &&
							    sametokpos(prev2, Corpus->prev2(count) /*tag_corpus_array[count-2]*/)) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "PREV2TAG") == 0) {
						if (count > 1) {
							if (sametokpos(tag, Corpus->prev2(count) /*tag_corpus_array[count-2]*/)) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "PREV2WD") == 0) {
						if (count > 1) {
							if (strcmp_nocase(word, Corpus->prev2(count)->getWord() /*word_corpus_array[count-2]*/) == 0) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "PREV1OR2TAG") == 0) {
						if (count > 0) {
							if (sametokpos(tag, Corpus->prev1(count) /*tag_corpus_array[count-1]*/) ||
							    sametokpos(tag, Corpus->prev2(count) /*tag_corpus_array[tempcount1]*/)) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "PREV1OR2WD") == 0) {
						if (count > 0) {
							if (strcmp_nocase(word, Corpus->prev1(count)->getWord() /*word_corpus_array[count-1]*/) == 0 ||
							    strcmp_nocase(word, Corpus->prev2(count)->getWord() /*word_corpus_array[tempcount1]*/) == 0) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "PREV1OR2OR3TAG") == 0) {
						if (count > 0) {
							if (sametokpos(tag, Corpus->prev1(count) /*tag_corpus_array[count-1]*/) ||
							    sametokpos(tag, Corpus->prevn(2, count) /*tag_corpus_array[tempcount1]*/) ||
							    sametokpos(tag, Corpus->prevn(3, count) /*tag_corpus_array[tempcount2]*/)) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "PREV1OR2OR3WD") == 0) {
						if (count > 0) {
							if (strcmp_nocase(word, Corpus->prev1(count)->getWord() /*word_corpus_array[count-1]*/) == 0 ||
							    strcmp_nocase(word, Corpus->prevn(2, count)->getWord() /*word_corpus_array[tempcount1]*/) == 0 ||
							    strcmp_nocase(word, Corpus->prevn(3, count)->getWord() /*word_corpus_array[tempcount2]*/) == 0) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else if (strcmp(when, "PREVBIGRAM") == 0) {
						if (count > 1) {
							if (sametokpos(prev2, Corpus->prev1(count) /*tag_corpus_array[count-1]*/) &&
							    sametokpos(prev1, Corpus->prev2(count) /*tag_corpus_array[count-2]*/)) {
								Corpus->Token[count].Pos = neww; /*change_the_tag(tag_corpus_array, neww, count);*/
							}
						}
					}
					else {
						fprintf(stderr,
						  "ERROR: %s is not an allowable transform type\n",
						  when.data());
					}
					if (Corpus->Token[count].Pos[0] & 0x80)
						printf("WHEN %s\n", when.data());
				}
			}
		}
	}
	substring::reset();
	if (Verbose) { // Bart 20030224
		fprintf(stderr, "\n");
	}

	Corpus->printUnsorted(fpout);

	return 0; /* Bart 20030415 */
}
