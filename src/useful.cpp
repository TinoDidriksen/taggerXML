#include "utf8func.h"
#include "useful.h"
#include <string.h>
#include <stdlib.h> // Bart 20001218
#include <stdio.h>  // Bart 20030224
#include <ctype.h>  // Bart 20030225

/* Set 'product' to the name of the environment variable that contains (part
   of the) path to the options file, not including the file name itself.
   If NULL, a file named 'xoptions' is assumed to be in the current working
   directory. 
   Default value: NULL*/
char* product = NULL;
/* Set 'relative_path_to_xoptions' to the path to the options file, including
   the file name itself and relative to the value stored in the <product>
   environment variable. 
   If NULL, a file named 'xoptions' is assumed to be in the directory
   specified in the <product> environment varaible. (or in the CWD, if 
   product == NULL.
   Default value: NULL */
char* relative_path_to_xoptions = NULL;
std::string xoptions;

int allocated = 0;

int strcmp_nocase(const char* a, const char* b) {
	int ret = strcmp(a, b);
	if (ret && !isUpperUTF8(a) && isUpperUTF8(b)) {
		ret = strcmp(a, allToLowerUTF8(b));
	}
	return ret;
}

int strcmp_nocase(std::string_view a, std::string_view b) {
	auto rv = a.compare(b);
	if (rv && !isUpperUTF8(a.data()) && isUpperUTF8(b.data())) {
		auto ab = &a.front(), ae = &a.back() + 1, bb = &b.front(), be = &b.back() + 1;
		while (ab < ae && bb < be) {
			bool status = true;
			auto ac = getUTF8char(ab, status);
			if (!status) {
				throw new std::runtime_error("Could not get code point from UTF-8 stream 'a'");
			}
			status = true;
			auto bc = getUTF8char(bb, status);
			if (!status) {
				throw new std::runtime_error("Could not get code point from UTF-8 stream 'b'");
			}
			if (ac == 0 && bc == 0) {
				return 0;
			}
			if (ac == bc) {
				continue;
			}
			if (ac == 0) {
				return -1;
			}
			if (bc == 0) {
				return 1;
			}
			ac = lowerEquivalent(ac);
			bc = lowerEquivalent(bc);
			if (ac == bc) {
				continue;
			}
			if (ac < bc) {
				return -1;
			}
			return 1;
		}
		if (ab == ae && bb == be) {
			return 0;
		}
	}
	return rv;
}

char* dupl(const char* s) {
	char* d = new char[strlen(s) + 1];
	strcpy(d, s);
	return d;
}

char* mystrdup(const char* thestr) {
	INCREMENT
	return strcpy((char*)malloc(strlen(thestr) + 1), thestr);
}

int not_just_blank(char* thestr)
/* make sure not just processing a no-character line */
{
	char* thestr2;
	thestr2 = thestr;
	while (*thestr2 != '\0') {
		if (*thestr2 != ' ' && *thestr2 != '\t' && *thestr2 != '\n') {
			return (1);
		}
		++thestr2;
	}
	return (0);
}

int num_words(char* thestr) {
	int count, returncount;

	returncount = 0;
	count = 0;
	while (thestr[count] != '\0' && (thestr[count] == ' ' || thestr[count] == '\t')) {
		++count;
	}
	while (thestr[count++] != '\0') {
		if (thestr[count - 1] == ' ' || thestr[count - 1] == '\t') {
			++returncount;
			while (thestr[count] == ' ' || thestr[count] == '\t') {
				++count;
			}
			if (thestr[count] == '\0') {
				--returncount;
			}
		}
	}
	return (returncount);
}

static const char* getXoptionsFile() {
	if (!xoptions.empty()) {
		if (!strcmp(xoptions.c_str(), "-")) { // Bart 20170213 -x- turns off use of xoptions
			return NULL;
		}
		return xoptions.c_str(); // Bart 20060321. New option -x <optionfile>
	}

	// loff
	static char basedir[1028];
	char* basedir_ptr;
	// fprintf(stderr,"Initialising, key: %s\n",key);
	if (product && (basedir_ptr = getenv(product /*"QA_DANISH"*/)) != NULL) {
		strcpy(basedir, basedir_ptr);
		if (relative_path_to_xoptions) {
			strcat(basedir, relative_path_to_xoptions /*"/preprocess/xoptions"*/);
		}
		else {
			strcat(basedir, "xoptions");
		}
		// fprintf(stderr,"Initialising using getenv %s\n",basedir);
	}
	else {
		strcpy(basedir, "xoptions");
	}
	// loff end

	//fprintf(stderr,"Initialising DAQPreprocess, using: %s\n",basedir);

	//loff FILE * fp = fopen("xoptions","rb");
	return basedir;
}

long option(const char* key) {
	auto XoptionsFile = getXoptionsFile();
	if (XoptionsFile) {
		FILE* fp = fopen(XoptionsFile, "rb");
		if (fp) {
			char line[256];
			while (fgets(line, 255, fp)) {
				if (!strncmp(key, line, strlen(key))) {
					char* eqpos = strchr(line, '=');
					if (eqpos) {
						fclose(fp);
						return strtol(eqpos + 1, NULL, 0);
					}
					else {
						break;
					}
				}
			}
			fclose(fp);
		}
	}
	return -1;
}

char* coption(const char* key) {
	//loff FILE * fp = fopen("xoptions","rb");
	FILE* fp = fopen(getXoptionsFile(), "rb");
	if (fp) {
		static /*Bart 20030224*/ char line[256];
		while (fgets(line, 255, fp)) {
			if (!strncmp(key, line, strlen(key))) {
				char* startp = strchr(line, '=');
				if (startp) {
					char* endp;
					while (isspace((unsigned char)*++startp))
						;
					for (endp = startp; *endp && !isspace((unsigned char)*endp); ++endp)
						;
					*endp = '\0';
					/*if(fp)
                    {
                    fprintf(fp,"%s = \"%s\"\n",key,startp);
                    fclose(fp);
                    }*/
					fclose(fp);
					return startp;
				}
				else {
					break;
				}
			}
		}
		fclose(fp);
	}
	return NULL;
}
