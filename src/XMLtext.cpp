/*
taggerXML - Brill's PoS tagger ported to C++ - can handle xml

Copyright (C) 2012 Center for Sprogteknologi, University of Copenhagen

(Files in this project without this header are 
 Copyright @ 1993 MIT and University of Pennsylvania)

This file is part of taggerXML.

taggerXML is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

taggerXML is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with taggerXML; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "field.h"
#include "XMLtext.h"
#include "word.h"
#include "parsesgml.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <string_view>

static void printXML(std::ostream& fpo, std::string_view s) {
	while (!s.empty()) {
		switch (s.front()) {
		case '<':
			fpo << "&lt;";
			break;
		case '>':
			fpo << "&gt;";
			break;
		case '&':
			if (s.find(';', 1) != std::string_view::npos)
				fpo << '&';
			else
				fpo << "&amp;";
			break;
		case '\'':
			fpo << "&apos;";
			break;
		case '"':
			fpo << "&quot;";
			break;
		default:
			fpo << s.front();
		}
		s.remove_prefix(1);
	}
}


corpus::corpus()
  : startOfLine(4532)
  , endOfLine(0)
  , numberOfTokens(0)
  , wordno(0)
  , Token(NULL)
  , CurrToken(NULL) {
	static char STAART[] = "STAART";
	staart.setWordpos(STAART, STAART + strlen(STAART));
	staart.setPOSpos(STAART, STAART + strlen(STAART));
	staart.Pos = STAART;
}

class crumb {
private:
	char* e;
	int L;
	crumb* next;

public:
	crumb(const char* s, int len, crumb* Crumbs)
	  : L(len)
	  , next(Crumbs) {
		e = new char[len + 1];
		strncpy(e, s, len);
		e[len] = '\0';
	}
	~crumb() {
		delete[] e;
	}
	const char* itsElement() {
		return e;
	}
	crumb* pop(const char* until, int len) {
		crumb* nxt = next;
		if (L == len && !strncmp(e, until, len)) {
			delete this;
			return nxt;
		}
		delete this;
		return nxt ? nxt->pop(until, len) : NULL;
	}
	void print() {
		if (next) {
			next->print();
		}
		printf("->%s", e);
	}
	bool is(std::string_view elm) {
		return (elm.compare(e) == 0);
	}
};

bool XMLtext::analyseThis() {
	if (!element.empty()) {
		return Crumbs && Crumbs->is(element);
	}
	if (!ancestor.empty()) {
		return Crumbs != NULL;
	}
	return true;
}


const char* XMLtext::convert(const char* s) {
	const char* p = strchr(s, '&');
	if (p && strchr(p + 1, ';')) {
		static char* ret[2] = { NULL, NULL };
		static size_t len[2] = { 0, 0 };
		static int index = 0;
		index += 1;
		index %= 2;
		if (len[index] <= strlen(s)) {
			delete ret[index];
			len[index] = strlen(s) + 1;
			ret[index] = new char[len[index]];
		}
		char* p = ret[index];
		while (*s) {
			if (*s == '&') {
				++s;
				if (!strncmp(s, "lt;", 3)) {
					*p++ = '<';
					s += 3;
				}
				else if (!strncmp(s, "gt;", 3)) {
					*p++ = '>';
					s += 3;
				}
				else if (!strncmp(s, "amp;", 4)) {
					*p++ = '&';
					s += 4;
				}
				else if (!strncmp(s, "apos;", 5)) // Notice: XML standard, but not HTML
				{
					*p++ = '\'';
					s += 5;
				}
				else if (!strncmp(s, "quot;", 5)) {
					*p++ = '"';
					s += 5;
				}
				else {
					*p++ = '&';
				}
			}
			else {
				*p++ = *s++;
			}
		}
		*p = '\0';
		return ret[index];
	}
	return s;
}


void XMLtext::CallBackStartElementName() {
	startElement = ch;
}

void XMLtext::CallBackEndElementName() {
	endElement = ch;
	if (ClosingTag) {
		if (Crumbs) {
			Crumbs = Crumbs->pop(startElement, endElement - startElement);
			if (!Crumbs) { // End of segment reached
				if (total > 0 && Token) {
					Token[total - 1].lastOfLine = true;
					Token[total].firstOfLine = true;
				}
			}
		}
		ClosingTag = false;
	}
	else {
		if (Crumbs || ancestor.empty() || (startElement + ancestor.size() == endElement && !strncmp(startElement, ancestor.data(), endElement - startElement))) {
			Crumbs = new crumb(startElement, endElement - startElement, Crumbs);
			if (!segment.empty() && total > 0 && Token && !strncmp(startElement, segment.data(), endElement - startElement)) {
				Token[total - 1].lastOfLine = true;
				Token[total].firstOfLine = true;
			}
		}
	}
}

void XMLtext::CallBackStartAttributeName() {
	if (analyseThis()) {
		ClosingTag = false;
		startAttributeName = ch;
	}
}

void XMLtext::CallBackEndAttributeNameCounting() {
	if (analyseThis()) {
		endAttributeName = ch;
		if (wordAttribute.size() == endAttributeName - startAttributeName && !strncmp(startAttributeName, wordAttribute.data(), wordAttribute.size())) {
			++total;
		}
	}
}

void XMLtext::CallBackEndAttributeNameInserting() {
	if (analyseThis()) {
		endAttributeName = ch;

		WordPosComing = false;
		PreTagPosComing = false;
		POSPosComing = false;


		if (wordAttribute.size() == endAttributeName - startAttributeName && !strncmp(startAttributeName, wordAttribute.data(), wordAttribute.size())) {
			WordPosComing = true;
		}
		else if (POSAttribute.size() == endAttributeName - startAttributeName && !strncmp(startAttributeName, POSAttribute.data(), POSAttribute.size())) {
			POSPosComing = true;
		}
		else if (PreTagAttribute.size() == endAttributeName - startAttributeName && !strncmp(startAttributeName, PreTagAttribute.data(), PreTagAttribute.size())) {
			PreTagPosComing = true;
		}
	}
}

void XMLtext::CallBackStartValue() {
	if (analyseThis()) {
		startValue = ch;
	}
}

void XMLtext::CallBackEndValue() {
	if (analyseThis()) {
		endValue = ch;
		if (WordPosComing) {
			Token[total].setWord(startValue, endValue, this);
		}
		else if (PreTagPosComing) {
			Token[total].setPreTagpos(startValue, endValue);
		}
		else if (POSPosComing) {
			Token[total].setPOSpos(startValue, endValue);
		}
	}
}

void XMLtext::CallBackNoMoreAttributes() {
	if (analyseThis()) {
		token* Tok = CurrToken;
		if (Tok && Tok->WordGetStart() != NULL && Tok->WordGetEnd() != Tok->WordGetStart()) { // Word as attribute
			char* EW = Tok->WordGetEnd();
			char savW = *EW;
			*EW = '\0';
			if (Tok->PreTagGetStart() != NULL) {
				char* EP = Tok->PreTagGetEnd();
				char savP = *EP;
				*EP = '\0';
				const char* pretag = convert(Tok->PreTagGetStart());
				Tok->Pos = Tok->PreTag = new char[strlen(pretag) + 1];
				strcpy(Tok->PreTag, pretag);
				*EP = savP;
			}
			*EW = savW;
			CurrToken = NULL;
		}
	}
}

void XMLtext::CallBackEndTag() {
	ClosingTag = true;
}

void XMLtext::CallBackEmptyTag() {
	if (Crumbs) {
		Crumbs = Crumbs->pop(startElement, endElement - startElement);
	}
}

class wordReader {
private:
	field* format;
	field* nextfield;
	field* wordfield;
	unsigned long newlines;
	unsigned long lineno;
	const char* tag;
	int kar;
	int lastkar;
	XMLtext* Text;
	char kars[2];

public:
	unsigned long getNewlines() {
		return newlines;
	}
	unsigned long getLineno() {
		return lineno;
	}
	void initWord() {
		nextfield = format;
		newlines = 0;
		format->reset();
		kars[1] = '\0';
		if (lastkar) {
			if (lastkar == '\n') {
				++newlines;
			}
			kars[0] = (char)lastkar;
			lastkar = 0;
			nextfield->read(kars, nextfield);
		}
		token* Tok = Text->getCurrentToken();
		if (Tok) {
			if (kars[0]) {
				Tok->setWordpos(Text->ch - 1, Text->ch - 1);
			}
			else {
				Tok->setWordpos(Text->ch, Text->ch);
			}
		}
	}
	wordReader(field* format, field* wordfield, XMLtext* Text)
	  : format(format)
	  , nextfield(format)
	  , wordfield(wordfield)
	  , newlines(0)
	  , lineno(0)
	  , tag(NULL)
	  , kar(EOF)
	  , lastkar(0)
	  , Text(Text) {
		initWord();
		assert(wordfield);
		if (lastkar) {
			assert(lastkar != EOF);
			if (lastkar == '\n') {
				++newlines;
			}
			kars[0] = (char)lastkar;
			lastkar = 0;
			nextfield->read(kars, nextfield);
		}
	}

	char* readChar(int kar) {
		if (!nextfield) {
			initWord();
		}
		if (kar == '\n') {
			++newlines;
		}
		kars[0] = kar == EOF ? '\0' : (char)kar;
		char* plastkar = nextfield->read(kars, nextfield);
		if (kar == '\0') {
			lastkar = '\0';
		}
		else if (plastkar) {
			lastkar = *plastkar;
		}

		if (nextfield) {
			return NULL;
		}
		lineno += newlines;
		return wordfield->getString();
	}

	char* count(int kar) {
		char* w = readChar(kar);
		if (Text->analyseThis()) {
			if (w && *w) {
				Text->incTotal();
			}
		}
		return w;
	}

	char* read(int kar) {
		char* w = readChar(kar);
		if (Text->analyseThis()) {
			if (w && *w) {
				token* Tok = Text->getCurrentToken();
				char* start = Tok->WordGetStart();
				Tok->setWord(start, start + strlen(w), Text);
				const char* pretag = Tok->PreTagGetStart();
				if (pretag != NULL) {
					char* EP = Tok->PreTagGetEnd();
					char savP = *EP;
					*EP = '\0';
					pretag = Text->convert(pretag);
					Tok->Pos = Tok->PreTag = new char[strlen(pretag) + 1];
					strcpy(Tok->PreTag, pretag);
					*EP = savP;
				}
				Text->CurrToken = NULL;
			}
		}
		return w;
	}
};

void XMLtext::printUnsorted(std::ostream& fpo) {
	if (alltext) {
		unsigned int k;
		const char* o = alltext;
		for (k = 0; k < total; ++k) {
			const char* start = Token[k].WordGetStart();
			const char* end = Token[k].WordGetEnd();
			const char* POSstart = Token[k].POSGetStart();
			const char* POSend = Token[k].POSGetEnd();

			auto p = Token[k].Pos;
			if (p.find(' ') != std::string_view::npos) {
				p = p.substr(0, p.find(' '));
			}
			if (POSstart == NULL) { // write word together with POS-tag
				if (start) {
					copy(fpo, o, end);
					o = end;
				}
				printXML(fpo, "/");
				printXML(fpo, Token[k].Pos);
			}
			else {
				copy(fpo, o, POSstart);
				o = POSend;
				printXML(fpo, Token[k].Pos);
			}
		}
		o = copy(fpo, o, o + strlen(o));
	}
}

token* XMLtext::getCurrentToken() {
	return Token ? Token + total : NULL;
}

void CallBackStartElementName(void* arg) {
	((XMLtext*)arg)->CallBackStartElementName();
}

void CallBackEndElementName(void* arg) {
	((XMLtext*)arg)->CallBackEndElementName();
}

void CallBackStartAttributeName(void* arg) {
	((XMLtext*)arg)->CallBackStartAttributeName();
}

static void (XMLtext::*CallBackEndAttributeNames)();

void CallBackEndAttributeName(void* arg) {
	(((XMLtext*)arg)->*CallBackEndAttributeNames)();
}

void CallBackStartValue(void* arg) {
	((XMLtext*)arg)->CallBackStartValue();
}

void CallBackEndValue(void* arg) {
	((XMLtext*)arg)->CallBackEndValue();
}

void CallBackNoMoreAttributes(void* arg) {
	((XMLtext*)arg)->CallBackNoMoreAttributes();
}

void CallBackEndTag(void* arg) {
	((XMLtext*)arg)->CallBackEndTag();
}

void CallBackEmptyTag(void* arg) {
	((XMLtext*)arg)->CallBackEmptyTag();
}

XMLtext::XMLtext(
  std::istream& fpi,
  std::string_view Iformat,
  bool XML,
  const std::string& ancestor,        // restrict POS-tagging to segments that fit in ancestor elements
  const std::string& segment,         // Optional segment delimiter. E.g. br: <br /> or s: <s> ... </s>
  const std::string& element,         // analyse PCDATA inside elements
  const std::string& wordAttribute,   // if null, word is PCDATA
  const std::string& PreTagAttribute, // Fetch pretagging from PreTagAttribute
  const std::string& POSAttribute     // Store POS in POSAttribute
  )
  : text()
  , ancestor(ancestor)
  , segment(segment)
  , element(element)
  , PreTagAttribute(PreTagAttribute)
  , POSAttribute(POSAttribute)
  , Crumbs(NULL)
  , ClosingTag(false)
  , WordPosComing(false)
  , PreTagPosComing(false)
  , POSPosComing(false)
  , wordAttribute(wordAttribute) {

#ifdef COUNTOBJECTS
	++COUNT;
#endif

	field* wordfield;
	field* format = 0;

	if (XML && !Iformat.empty()) {
		fpi.seekg(0, std::ios::end);
		long filesize = static_cast<long>(fpi.tellg());
		fpi.seekg(0, std::ios::beg);
		if (filesize > 0) {
			alltext = new char[filesize + 1];
			char* p = alltext;
			char* e = alltext + filesize;
			while (p < e) {
				char kar;
				fpi >> kar;
				*p++ = kar;
			}
			*p = '\0';
			fpi.seekg(0, std::ios::beg);
		}
	}
	if (alltext) {
		parseAsXml();
		html_tag_class html_tag(this);
		CallBackEndAttributeNames = &XMLtext::CallBackEndAttributeNameCounting;
		if (!element.empty() || !ancestor.empty()) {
			html_tag.setCallBackStartElementName(::CallBackStartElementName);
			html_tag.setCallBackEndElementName(::CallBackEndElementName);
			html_tag.setCallBackEndTag(::CallBackEndTag);
			html_tag.setCallBackEmptyTag(::CallBackEmptyTag);
		}
		else {
			html_tag.setCallBackStartElementName(::dummyCallBack);
			html_tag.setCallBackEndElementName(::dummyCallBack);
			html_tag.setCallBackEndTag(::dummyCallBack);
			html_tag.setCallBackEmptyTag(::dummyCallBack);
		}
		if (!wordAttribute.empty() || !POSAttribute.empty() || !PreTagAttribute.empty()) {
			html_tag.setCallBackStartAttributeName(::CallBackStartAttributeName);
			html_tag.setCallBackEndAttributeName(::CallBackEndAttributeName);
		}
		else {
			html_tag.setCallBackStartAttributeName(::dummyCallBack);
			html_tag.setCallBackEndAttributeName(::dummyCallBack);
		}
		html_tag.setCallBackStartValue(::dummyCallBack);
		html_tag.setCallBackEndValue(::dummyCallBack);
		html_tag.setCallBackNoMoreAttributes(::dummyCallBack);
		ch = alltext;
		char* curr_pos = alltext;
		char* endpos = alltext;
		estate Seq = notag;

		char* (wordReader::*fnc)(int kar);
		int loop;
		fnc = &wordReader::count;
		wordfield = 0;
		if (!Iformat.empty()) {
			format = translateFormat(Iformat, wordfield);
			if (!wordfield) {
				printf("Input format %s must specify '$w'.\n", Iformat.data());
				exit(0);
			}
		}
		for (loop = 1; loop <= 2; ++loop) {
			wordReader WordReader(format, wordfield, this);
			while (*ch) {
				while (*ch && ((Seq = (html_tag.*tagState)(*ch)) == tag || Seq == endoftag_startoftag)) {
					ch++;
				}
				if (Seq == notag) { // Not an HTML tag. Backtrack.
					endpos = ch;
					ch = curr_pos;
					while (ch < endpos) {
						(WordReader.*fnc)(*ch);
						ch++;
					}
				}
				else if (Seq == endoftag) {
					++ch; // skip >
				}
				if (*ch) {
					while (*ch && (Seq = (html_tag.*tagState)(*ch)) == notag) {
						(WordReader.*fnc)(*ch);
						++ch;
					}
					(WordReader.*fnc)('\0');
					if (Seq == tag) {
						(WordReader.*fnc)('\0');
						curr_pos = ch++; // skip <
					}
				}
			}
			if (Seq == tag) { // Not an SGML tag. Backtrack.
				endpos = ch;
				ch = curr_pos;
				while (ch < endpos) {
					(WordReader.*fnc)(*ch);
					ch++;
				}
			}
			if (loop == 1) {
				numberOfTokens = total;
				Token = new token[total + 1]; // 1 extra for the epilogue after the last token

				total = 0;
				ch = alltext;
				curr_pos = alltext;
				fnc = &wordReader::read;
				CallBackEndAttributeNames = &XMLtext::CallBackEndAttributeNameInserting;
				if (!wordAttribute.empty() || !PreTagAttribute.empty() || !POSAttribute.empty()) {
					html_tag.setCallBackStartAttributeName(::CallBackStartAttributeName);
					html_tag.setCallBackEndAttributeName(::CallBackEndAttributeName);
					html_tag.setCallBackStartValue(::CallBackStartValue);
					html_tag.setCallBackEndValue(::CallBackEndValue);
					html_tag.setCallBackNoMoreAttributes(::CallBackNoMoreAttributes);
				}
			}
		}
	}
}
