#ifndef TEXT_H
#define TEXT_H

#include "defines.h"
#include "substring.h"
#include <string>
#include <string_view>
#include <iostream>

#define ESCAPESLASH 0

const char* copy(std::ostream& fp, const char* o, const char* end);

class strng;
class text;

class token {
private:
	substring Wordpos;
	substring POSpos;
	substring PreTagpos;

public:
	strng* wordPrePos;
	char* PreTag;         // does own. Tag(s) from input
	std::string_view Pos; // does not own. Generated tags. Part of rule text.
	bool firstOfLine : 1;
	bool lastOfLine : 1;

	void setWordpos(char* start, char* end) {
		Wordpos.set(start, end);
	}
	void setWord(char* start, char* end, text* Text);
	void setPOSpos(char* start, char* end) {
		POSpos.set(start, end);
	}
	void setPreTagpos(char* start, char* end) {
		PreTagpos.set(start, end);
	}
	char* WordGetStart() {
		return Wordpos.getStart();
	}
	char* WordGetEnd() {
		return Wordpos.getEnd();
	}
	char* POSGetStart() {
		return POSpos.getStart();
	}
	char* POSGetEnd() {
		return POSpos.getEnd();
	}
	char* PreTagGetStart() {
		return PreTagpos.getStart();
	}
	char* PreTagGetEnd() {
		return PreTagpos.getEnd();
	}
	const char* getWord();

	token();
	~token() {
		delete[] PreTag;
	}
};

class Word;
class taggedWord;
class basefrm;

struct tallyStruct {
	unsigned long int totcnt = 0;
	unsigned long int totcntTypes = 0;
	unsigned long int newcnt = 0;
	unsigned long int newcntTypes = 0;
	unsigned long int newhom = 0;
	unsigned long int newhomTypes = 0;
};

class field;

class corpus {
public:
	token staart;
	int startOfLine;
	int endOfLine;
	int numberOfTokens;
	int wordno;
	token* Token;
	token* CurrToken;
	void rewind() {
		startOfLine = 1234;
		endOfLine = -1;
		substring::reset();
	}
	const char* getWord() {
		if (++wordno <= endOfLine) {
			return Token[wordno].getWord();
		}
		wordno = startOfLine - 1;
		return NULL;
	}

	token* prev1(int count) {
		if (count <= 0 || Token[count].firstOfLine) {
			return &staart;
		}
		return Token + count - 1;
	}
	token* prev2(int count) {
		if (count <= 1 || Token[count].firstOfLine || Token[count - 1].firstOfLine) {
			return &staart;
		}
		return Token + count - 2;
	}
	token* prevn(int n, int count) {
		if (count < n) {
			return &staart;
		}
		for (int i = 0; i < n; ++i) {
			if (Token[count - i].firstOfLine) {
				return &staart;
			}
		}
		return Token + count - n;
	}
	token* next1(int count) {
		if (count >= numberOfTokens - 1 || Token[count].lastOfLine) {
			return &staart;
		}
		return Token + count + 1;
	}
	token* next2(int count) {
		if (count >= numberOfTokens - 2 || Token[count].lastOfLine || Token[count + 1].lastOfLine) {
			return &staart;
		}
		return Token + count + 2;
	}
	token* nextn(int n, int count) {
		if (count >= numberOfTokens - n) {
			return &staart;
		}
		for (int i = 0; i < n; ++i) {
			if (Token[count + i].lastOfLine) {
				return &staart;
			}
		}
		return Token + count + n;
	}
	bool getline() {
		startOfLine = ++endOfLine;
		wordno = startOfLine - 1;
		if (numberOfTokens > startOfLine) {
			while (!Token[endOfLine].lastOfLine && endOfLine < numberOfTokens) {
				++endOfLine;
			}
			return true;
		}
		return false;
	}
	corpus();
	~corpus() {
		delete[] Token;
	}
};


class text : public corpus {
#ifdef COUNTOBJECTS
public:
	static int COUNT;
#endif
private:
#if ESCAPESLASH
	virtual const char* convert(const char* s);
#else
	virtual const char* convert(const char* s) {
		return s;
	}
#endif
protected:
	char* alltext;
	size_t N;
	unsigned long int total;
	field* fields;
	void AddField(field* fld);
	field* translateFormat(std::string_view Iformat, field*& wordfield);
	void insert(const char* w, const char* tag);

public:
	char* ch;
	int newcnt;
	void incTotal() {
		++total;
	}
	void incTotal(unsigned long int inc) {
		total += inc;
	}
	text();
	text(std::istream& fpi, bool FINAL_ONLY_FLAG);
	virtual ~text();
	strng* insert(const char* w);
	virtual void printUnsorted(std::ostream& fpo);
};

#endif
