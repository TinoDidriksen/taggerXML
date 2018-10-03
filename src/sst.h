#ifndef SST_H
#define SST_H

#include "hashmap.h"
#include "XMLtext.h"
#include "newregistry.h"
#include "defines.h"
#include <assert.h>
#include <string>
#include <string_view>

class strng {
private:
	std::string Key;
	std::string Val;

public:
	strng(const char* key, const char* val) {
		assert(key != 0);
		Key.assign(key);
		if (val) {
			Val.assign(val);
		}
	}
	const char* key() const {
		return Key.c_str();
	}
	const char* val() const {
		return Val.c_str();
	}
	void setVal(const char* val) {
		Val.clear();
		if (val) {
			Val.assign(val);
		}
	}
	void setVal(std::string_view val) {
		Val.assign(val);
	}
};

struct optionStruct;

int start_state_tagger(
  NewRegistry& lexicon_hash,
  corpus* Corpus,
  SV2_Set& bigram_hash,
  NewDarray& rule_array,
  SV_Set& wordlist_hash,
  optionStruct* Options,
  hashmap::hash<strng>* tag_hash);

#endif
