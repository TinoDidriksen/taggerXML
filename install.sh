#!/bin/bash

if [[ ! -s "install.sh" ]]
then
	echo "Must be in the taggerXML root folder to run this script!"
	exit 1
fi

rm -rf build
mkdir -pv deps build

pushd deps
	if [[ ! -s "hashmap/src/hashmap.h" ]]
	then
		git clone https://github.com/kuhumcst/hashmap.git
	fi
	if [[ ! -s "letterfunc/src/letterfunc.h" ]]
	then
		git clone https://github.com/kuhumcst/letterfunc.git
	fi
	if [[ ! -s "parsesgml/src/parsesgml.h" ]]
	then
		git clone https://github.com/kuhumcst/parsesgml.git
	fi
popd

pushd build
	cmake ..
	make -j4 V=1 VERBOSE=1
	echo "Installing with sudo - may need to input your sudo password here."
	sudo make install
popd
