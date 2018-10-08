/* Copyright @ 1993 MIT and University of Pennsylvania */
/* Written by Eric Brill */
/*THIS SOFTWARE IS PROVIDED "AS IS", AND M.I.T. MAKES NO REPRESENTATIONS 
OR WARRANTIES, EXPRESS OR IMPLIED.  By way of example, but not 
limitation, M.I.T. MAKES NO REPRESENTATIONS OR WARRANTIES OF 
MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR THAT THE USE OF 
THE LICENSED SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY 
PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.   */

/* Adapted by Bart Jongejan. (Conversion to Standard C and more.) */

#include "XMLtext.h"
#include "newregistry.h"
#include "useful.h"
#include "lex.h"
#include "sst.h"
#include "fst.h"
#include "staart.h"
#include "readcorpus.h"
#include "writcorp.h"
#include "tagger.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fstream>

lmdb::env lexicon_mdb{ lmdb::env::create() };
lmdb::env bigram_mdb{ lmdb::env::create() };
lmdb::env wordlist_mdb{ lmdb::env::create() };
mmap_region lrf_mmap;

static std::string Contextualrulefile;
static bool START_ONLY_FLAG = false;
static bool FINAL_ONLY_FLAG = false;

static int corpussize = 0;
static int linenums = 0;
static int tagnums = 0;
static NewDarray RULE_ARRAY;

#if RESTRICT_MOVE
#	if WITHSEENTAGGING
static Registry SEENTAGGING;
#	endif
#	if WITHWORDS
static Registry WORDS;
#	endif
#endif

bool createRegistries(
  std::string_view optpath,
#if RESTRICT_MOVE
#	if WITHSEENTAGGING
  Registry* SEENTAGGING,
#	endif
#	if WITHWORDS
  Registry* WORDS,
#	endif
#endif
  std::string Lexicon,
  std::string Lexicalrulefile,
  std::string Bigrams,
  std::string Wordlist,
#if !RESTRICT_MOVE
  int START_ONLY_FLAG,
#endif
  NewDarray& RULE_ARRAY) {
	NewDarray& rule_array{ RULE_ARRAY };

	auto load_data = [&](lmdb::env& env, std::string filename, auto perline) {
		filename = find_file(optpath, filename);

		struct stat _stat {};
		stat(filename.c_str(), &_stat);
		auto data_mdb = _stat.st_mtime;
		auto data_size = _stat.st_size;

		std::string tmp{ filename };
		tmp.append(".mdb");
		auto stat_err = stat(tmp.c_str(), &_stat);

		env.open(tmp.c_str(), MDB_NOSUBDIR | MDB_WRITEMAP | MDB_NOSYNC, 0664);
		env.set_mapsize(data_size * 4);

		// If lexicon data file is newer or the database is empty, (re)create database
		MDB_stat mdbs;
		lmdb::env_stat(env, &mdbs);
		if (stat_err != 0 || data_mdb > _stat.st_mtime || mdbs.ms_entries == 0) {
			auto wtxn = lmdb::txn::begin(env);
			auto dbi = lmdb::dbi::open(wtxn);
			dbi.drop(wtxn); // Drop all existing entries, if any
			wtxn.commit();

			lmdb_writer wr(env);

			auto file = mmap_region{ filename };
			for (std::string_view line{ nextline(file) }; !line.empty(); line = nextline(file, line)) {
				perline(wr, line);
			}
			wr.txn.commit();
		}
	};

	auto load_lex = [&](lmdb_writer& wr, std::string_view line) {
		lmdb::val key;
		lmdb::val value;
		auto strs = split<2>(line);
		if (!strs[0].empty() && !strs[1].empty()) {
			key.assign(strs[0].data(), strs[0].size());
			value.assign(strs[1].data(), strs[1].size());
			wr.dbi.put(wr.txn, key, value, MDB_NODUPDATA | MDB_NOOVERWRITE);
		}
	};

	load_data(lexicon_mdb, Lexicon, load_lex);

	/* read in rule file */
	SV_Set good_right_hash;
	SV_Set good_left_hash;

	Lexicalrulefile = find_file(optpath, Lexicalrulefile);
	lrf_mmap = mmap_region{ Lexicalrulefile };
	for (std::string_view line{ nextline(lrf_mmap) }; !line.empty(); line = nextline(lrf_mmap, line)) {
		auto perl_split_ptr = split<6>(line);
		rule_array.emplace_back(perl_split_ptr);
		if (perl_split_ptr[1].compare("goodright") == 0) {
			good_right_hash.emplace(perl_split_ptr[0]);
		}
		else if (perl_split_ptr[2].compare("fgoodright") == 0) {
			good_right_hash.emplace(perl_split_ptr[1]);
		}
		else if (perl_split_ptr[1].compare("goodleft") == 0) {
			good_left_hash.emplace(perl_split_ptr[0]);
		}
		else if (perl_split_ptr[2].compare("fgoodleft") == 0) {
			good_left_hash.emplace(perl_split_ptr[1]);
		}
	}

	/* read in bigram file */
	std::string tmp;
	auto load_bigram = [&](lmdb_writer& wr, std::string_view line) {
		auto bigram = split<2>(line);
		if (good_right_hash.count(bigram[0]) || good_left_hash.count(bigram[1])) {
			join(tmp, bigram[0], '\t', bigram[1]);
			lmdb::val key;
			key.assign(tmp.c_str(), tmp.size());
			lmdb::val value{};
			wr.dbi.put(wr.txn, key, value, MDB_NODUPDATA | MDB_NOOVERWRITE);
		}
	};

	load_data(bigram_mdb, Bigrams, load_bigram);

	if (!Wordlist.empty()) {
		auto load_words = [&](lmdb_writer& wr, std::string_view line) {
			lmdb::val key;
			key.assign(line.data(), line.size());
			lmdb::val value{};
			wr.dbi.put(wr.txn, key, value, MDB_NODUPDATA | MDB_NOOVERWRITE);
		};
		load_data(wordlist_mdb, Wordlist, load_words);
	}

	return true;
}

tagger::tagger() {
	Contextualrulefile.clear();
	corpussize = 0;
	linenums = 0;
	tagnums = 0;
#if RESTRICT_MOVE
#	if WITHSEENTAGGING
	SEENTAGGING = NULL;
#	endif
#	if WITHWORDS
	WORDS = NULL;
#	endif
#endif
	RULE_ARRAY.clear();
}

bool tagger::init(
  std::string_view optpath,
  const std::string& _Lexicon,
  const std::string& _Bigrams,
  const std::string& _Lexicalrulefile,
  const std::string& _Contextualrulefile,
  const std::string& _wdlistname,
  bool _START_ONLY_FLAG,
  bool _FINAL_ONLY_FLAG) {
	Contextualrulefile = _Contextualrulefile;
	START_ONLY_FLAG = _START_ONLY_FLAG;
	FINAL_ONLY_FLAG = _FINAL_ONLY_FLAG;

	if ((START_ONLY_FLAG && FINAL_ONLY_FLAG) || (FINAL_ONLY_FLAG && !_wdlistname.empty())) {
		fprintf(stderr, "This set of options does not make sense.\n");
		return false;
	}


	return createRegistries(
	  optpath,
#if RESTRICT_MOVE
#	if WITHSEENTAGGING
	  &SEENTAGGING,
#	endif
#	if WITHWORDS
	  &WORDS,
#	endif
#endif
	  _Lexicon,
	  _Lexicalrulefile,
	  _Bigrams,
	  _wdlistname,
#if !RESTRICT_MOVE
	  START_ONLY_FLAG,
#endif
	  RULE_ARRAY);
}


bool tagger::analyse(std::istream& CORPUS, std::ostream& fpout, optionStruct* Options) {
	text* Text = NULL;
	if (Options->XML) {
		Text = new XMLtext(CORPUS, "$w\\s", //char * Iformat
		  true,                             //bool XML
		  Options->ancestor,                //"p"//const char * ancestor // restrict POS-tagging to segments that fit in ancestor elements
		  Options->segment,
		  Options->element,         //"w"//const char * element // analyse PCDATA inside elements
		  Options->wordAttribute,   //NULL//const char * wordAttribute // if null, word is PCDATA
		  Options->PreTagAttribute, // Store POS in PreTagAttribute
		  Options->POSAttribute     //"lemma"//const char * POSAttribute // if null, Lemma is PCDATA
		);
	}
	else {
		Text = new text(CORPUS, FINAL_ONLY_FLAG);
	}

	hashmap::hash<strng>* tag_hash;
	tag_hash = new hashmap::hash<strng>(&strng::key, 1000);
	if (!FINAL_ONLY_FLAG) {
		corpussize = -1 + start_state_tagger(
		                    lexicon_mdb,
		                    Text,
		                    bigram_mdb,
		                    RULE_ARRAY,
		                    wordlist_mdb,
		                    Options,
		                    tag_hash);
	}
	if (!START_ONLY_FLAG) {
		final_state_tagger(Contextualrulefile,
#if RESTRICT_MOVE
#	if WITHSEENTAGGING
		  SEENTAGGING,
#	endif
#	if WITHWORDS
		  WORDS,
#	else
		  lexicon_mdb,
#	endif
#endif
		  Text, Options, fpout);
	}
	else {
		Text->printUnsorted(fpout);
	}
	tag_hash->deleteThings();
	delete tag_hash;
	delete Text;
	return true;
}
