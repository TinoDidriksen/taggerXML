# Boost
find_path(Boost_LOCAL NAMES boost/config.hpp PATHS "${CMAKE_CURRENT_SOURCE_DIR}/../include" NO_DEFAULT_PATH)
if(Boost_LOCAL)
	set(BOOST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../include")
	find_package(Boost 1.53.0)
endif()
if(NOT Boost_FOUND)
	unset(BOOST_ROOT)
	message(STATUS "Did not find Boost locally; trying globally...")
	message(STATUS "If this doesn't work, run ./get-boost.sh and try again.")
	find_package(Boost 1.53.0 REQUIRED)
endif()
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

if(VCPKG_TOOLCHAIN)
	find_package(lmdb CONFIG REQUIRED)
else()
	find_library(HAS_LMDB NAMES lmdb)
endif()

set(TAGGERXML_HEADERS
	argopt.h
	bool.h
	defines.h
	field.h
	fst.h
	"${CMAKE_SOURCE_DIR}/deps/hashmap/src/hashmap.h"
	lex.h
	memory.h
	option.h
	"${CMAKE_SOURCE_DIR}/deps/parsesgml/src/parsesgml.h"
	readcorpus.h
	newregistry.h
	sst.h
	staart.h
	substring.h
	sysdep.h
	tagger.h
	text.h
	useful.h
	"${CMAKE_SOURCE_DIR}/deps/letterfunc/src/utf8func.h"
	word.h
	writcorp.h
	XMLtext.h
	"${CMAKE_SOURCE_DIR}/deps/lmdb++.h"
	)

set(TAGGERXML_SOURCES
	argopt.cpp
	field.cpp
	fst.cpp
	"${CMAKE_SOURCE_DIR}/deps/hashmap/src/hashmap.cpp"
	"${CMAKE_SOURCE_DIR}/deps/letterfunc/src/letter.cpp"
	"${CMAKE_SOURCE_DIR}/deps/letterfunc/src/letterfunc.cpp"
	lex.cpp
	main.cpp
	option.cpp
	"${CMAKE_SOURCE_DIR}/deps/parsesgml/src/parsesgml.cpp"
	readcorpus.cpp
	sst.cpp
	substring.cpp
	tagger.cpp
	text.cpp
	useful.cpp
	"${CMAKE_SOURCE_DIR}/deps/letterfunc/src/utf8func.cpp"
	word.cpp
	XMLtext.cpp
	${TAGGERXML_HEADERS}
	)

include_directories(
	${LMDB_INCLUDE_DIR}
	"${CMAKE_SOURCE_DIR}/src"
	"${CMAKE_SOURCE_DIR}/deps"
	"${CMAKE_SOURCE_DIR}/deps/hashmap/src"
	"${CMAKE_SOURCE_DIR}/deps/letterfunc/src"
	"${CMAKE_SOURCE_DIR}/deps/parsesgml/src"
	)

add_executable(taggerXML ${TAGGERXML_SOURCES})
target_link_libraries(taggerXML PRIVATE lmdb)

install(TARGETS taggerXML RUNTIME DESTINATION bin)
