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
#include "registry.h"
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

#define BOOST_DATE_TIME_NO_LIB 1
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

#include <fstream>

namespace bi = ::boost::interprocess;
struct mmap_region {
    bi::file_mapping fmap;
    bi::mapped_region mreg;
    const char *buf;
    size_t size;

    mmap_region() {}

    mmap_region(const char *fname)
        : fmap(fname, bi::read_only)
        , mreg(fmap, bi::read_only)
        , buf(reinterpret_cast<const char*>(mreg.get_address()))
        , size(mreg.get_size())
    {}
};
mmap_region lexicon_mmap;
mmap_region lrf_mmap;
mmap_region bigram_mmap;
mmap_region wordlist_mmap;

template<typename C>
inline auto is_space(const C& c) {
    return ((c == ' ') || (c == '\t') || (c == '\r') || (c == '\n'));
}

inline auto nextline(mmap_region& reg, std::string_view line = {}) {
    const char *start = reg.buf;
    const char *end = reg.buf + reg.size;

    // If we were already reading lines, start from the previous line's end
    if (!line.empty()) {
        start = &line.back() + 1;
    }

    // Skip leading space, which also skips the newline that the old back() might be parked behind
    // Also skips empty lines, which is desired behavior
    while (start < end && is_space(*start)) {
        ++start;
    }
    if (start >= end) {
        return std::string_view{};
    }

    // Find the next newline...
    const char *stop = start;
    while (stop < end && *stop != '\n') {
        ++stop;
    }
    // ...but trim trailing whitespace
    while (stop > start && is_space(stop[-1])) {
        --stop;
    }

    return std::string_view{ start, static_cast<size_t>(stop - start) };
};

template<size_t N>
inline auto split(std::string_view str) {
    std::array<std::string_view, N> strs;

    while (!str.empty() && is_space(str.front())) {
        str.remove_prefix(1);
    }

    auto start = &str.front();
    auto end = &str.back() + 1;

    // Split the first N-1 nicely...
    for (size_t n = 0; n < N - 1 && start < end; ++n) {
        auto stop = start;
        while (stop < end && !is_space(*stop)) {
            ++stop;
        }

        strs[n] = std::string_view{ start, static_cast<size_t>(stop - start) };

        start = stop;
        while (start < end && is_space(*start)) {
            ++start;
        }
    }
    // ...but the last part should just contain whatever is left in the input
    strs[N - 1] = std::string_view{ start, static_cast<size_t>(end - start) };

    return strs;
}

static const char * Contextualrulefile;
static bool START_ONLY_FLAG;
static bool FINAL_ONLY_FLAG;

static int corpussize;
static int linenums;
static int tagnums;
static NewRegistry LEXICON_HASH;
static SV2_Set BIGRAM_HASH;
static SV_Set WORDLIST_HASH;
static NewDarray RULE_ARRAY;

#if RESTRICT_MOVE
#if WITHSEENTAGGING
static Registry SEENTAGGING;
#endif
#if WITHWORDS
static Registry WORDS;
#endif
#endif

bool createRegistries
        (
#if RESTRICT_MOVE
#if WITHSEENTAGGING
        Registry * SEENTAGGING,
#endif
#if WITHWORDS
        Registry * WORDS,
#endif
#endif
        NewRegistry& LEXICON_HASH,
        SV2_Set& BIGRAM_HASH,
        SV_Set& WORDLIST_HASH,
        const char * Lexicon,
        const char * Lexicalrulefile,
        const char * Bigrams,
        const char * Wordlist,
#if !RESTRICT_MOVE
        int START_ONLY_FLAG,
#endif
        NewDarray& RULE_ARRAY
        )
    {
    NewRegistry& lexicon_hash{ LEXICON_HASH };
    SV2_Set& bigram_hash{ BIGRAM_HASH };
    SV_Set& wordlist_hash{ WORDLIST_HASH };
    NewDarray& rule_array{ RULE_ARRAY };

    lexicon_mmap = mmap_region{ Lexicon };
    for (std::string_view line{ nextline(lexicon_mmap) }; !line.empty(); line = nextline(lexicon_mmap, line)) {
        auto strs = split<2>(line);
        if (!strs[0].empty() && !strs[1].empty()) {
            lexicon_hash.emplace(std::make_pair(strs[0], strs[1]));
        }
    }

    /* read in rule file */
    SV_Set good_right_hash;
    SV_Set good_left_hash;
    
    lrf_mmap = mmap_region{ Lexicalrulefile };
    for (std::string_view line{ nextline(lexicon_mmap) }; !line.empty(); line = nextline(lexicon_mmap, line)) {
        auto perl_split_ptr = split<6>(line);
        rule_array.emplace_back(perl_split_ptr);
        if (perl_split_ptr[1].compare("goodright") == 0) 
            {
            good_right_hash.emplace(perl_split_ptr[0]);
            }
        else if (perl_split_ptr[2].compare("fgoodright") == 0)
            {
            good_right_hash.emplace(perl_split_ptr[1]);
            }
        else if (perl_split_ptr[1].compare("goodleft") == 0)
            {
            good_left_hash.emplace(perl_split_ptr[0]);
            }
        else if (perl_split_ptr[2].compare("fgoodleft") == 0)
            {
            good_left_hash.emplace(perl_split_ptr[1]);
            }
    }

    /* read in bigram file */
    bigram_mmap = mmap_region{ Bigrams };
    for (std::string_view line{ nextline(lexicon_mmap) }; !line.empty(); line = nextline(lexicon_mmap, line)) {
        auto bigram = split<2>(line);
        if (good_right_hash.count(bigram[0]))
            {
            bigram_hash.emplace(bigram);
            }
        if (good_left_hash.count(bigram[1])) 
            {
            bigram_hash.emplace(bigram);
            }
        }

    if (Wordlist) 
        {
        wordlist_mmap = mmap_region{ Wordlist };
        for (std::string_view line{ nextline(lexicon_mmap) }; !line.empty(); line = nextline(lexicon_mmap, line)) {
            wordlist_hash.emplace(line);
            }
        }
    return true;
    }

static void deleteRegistries(
#if RESTRICT_MOVE
#if WITHSEENTAGGING
        Registry SEENTAGGING,
#endif
#if WITHWORDS
        Registry WORDS,
#endif
#endif
                             )
    {
#if RESTRICT_MOVE
#if WITHSEENTAGGING
    freeRegistry(SEENTAGGING);
#endif
#if WITHWORDS
    freeRegistry(WORDS);
#endif
#endif
    }

tagger::tagger()
    {
    Contextualrulefile = NULL;
    corpussize = 0;
    linenums = 0;
    tagnums = 0;
#if RESTRICT_MOVE
#if WITHSEENTAGGING
    SEENTAGGING = NULL;
#endif
#if WITHWORDS
    WORDS = NULL;
#endif
#endif
    LEXICON_HASH.clear();
    BIGRAM_HASH.clear();
    WORDLIST_HASH.clear();
    RULE_ARRAY.clear();
    }

bool tagger::init(
                const char * _Lexicon,
                const char * _Bigrams,
                const char * _Lexicalrulefile,
                const char * _Contextualrulefile,
                const char * _wdlistname,
                bool _START_ONLY_FLAG,
                bool _FINAL_ONLY_FLAG
                )
    {
    Contextualrulefile = _Contextualrulefile;
    START_ONLY_FLAG = _START_ONLY_FLAG;
    FINAL_ONLY_FLAG = _FINAL_ONLY_FLAG;
    if (  (  START_ONLY_FLAG 
          && FINAL_ONLY_FLAG
          ) 
       || (  FINAL_ONLY_FLAG 
          && _wdlistname
          ) 
       ) 
        {
        fprintf(stderr,"This set of options does not make sense.\n");
        return false;
        }
        

    return createRegistries
        (
#if RESTRICT_MOVE
#if WITHSEENTAGGING
        &SEENTAGGING, 
#endif
#if WITHWORDS
        &WORDS,
#endif
#endif
        LEXICON_HASH,
        BIGRAM_HASH,
        WORDLIST_HASH,
        _Lexicon,
        _Lexicalrulefile,
        _Bigrams,
        _wdlistname,
#if !RESTRICT_MOVE
        START_ONLY_FLAG,
#endif
        RULE_ARRAY
        );
    }


bool tagger::analyse(std::istream & CORPUS,std::ostream & fpout,optionStruct * Options)
    {
    text * Text = NULL;
    if(Options->XML)
        {
        Text = new XMLtext
           (CORPUS
           ,"$w\\s"//char * Iformat
           ,true//bool XML
           ,Options->ancestor//"p"//const char * ancestor // restrict POS-tagging to segments that fit in ancestor elements
           ,Options->segment
           ,Options->element//"w"//const char * element // analyse PCDATA inside elements
           ,Options->wordAttribute//NULL//const char * wordAttribute // if null, word is PCDATA
           ,Options->PreTagAttribute // Store POS in PreTagAttribute
           ,Options->POSAttribute//"lemma"//const char * POSAttribute // if null, Lemma is PCDATA
           );
        }
    else
        {
        Text = new text(CORPUS,FINAL_ONLY_FLAG);
        }

    hashmap::hash<strng> * tag_hash;
    tag_hash = new hashmap::hash<strng>(&strng::key,1000);
    if(!FINAL_ONLY_FLAG) 
        {
        corpussize = -1 + start_state_tagger
             (
             LEXICON_HASH,
             Text,
             BIGRAM_HASH,
             RULE_ARRAY,
             WORDLIST_HASH,
             Options,
             tag_hash
             );
        }
    if(!START_ONLY_FLAG)
         {
         final_state_tagger(Contextualrulefile
#if RESTRICT_MOVE
#if WITHSEENTAGGING
                           , SEENTAGGING
#endif
#if WITHWORDS
                           , WORDS
#else
                           , LEXICON_HASH
#endif
#endif
                           ,Text
                           ,Options
                           ,fpout
                           );
        }
    else
        Text->printUnsorted(fpout);
    tag_hash->deleteThings();
    delete tag_hash;
    delete Text;
    return true;
    }
