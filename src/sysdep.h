#ifndef _SYSDEP_H_
#define _SYSDEP_H_

#ifdef _MSC_VER
// warning C4512: assignment operator could not be generated
#	pragma warning(disable : 4512)
// warning C4456: declaration hides previous local declaration
#	pragma warning(disable : 4456)
// warning C4458: declaration hides class member
#	pragma warning(disable : 4458)
// warning C4312: 'operation' : conversion from 'type1' to 'type2' of greater size
#	pragma warning(disable : 4312)
#endif

#include <sys/stat.h>
#include <string>
#include <string_view>

inline std::string find_file(std::string_view path, std::string name) {
	struct stat _stat;
	auto err = stat(name.c_str(), &_stat);
	if (err == 0) {
		return name;
	}

	name.insert(0, path);
	return name;
}

#define NORET void

/* CONSTVOIDP is for pointers to non-modifyable void objects */

typedef const void* CONSTVOIDP;
typedef void* VOIDP;
#define NOARGS void
#define PROTOTYPE(x) x

#endif /* ifndef _SYSDEP_H_ */
