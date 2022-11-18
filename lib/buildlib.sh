#!/bin/sh

mkdir -p include
mkdir -p link

for l in lib*
do
	(
	cd "$l" || exit
	if [ "$1" = 'clean' ]; then
		make clean
	else
		make
		cp -- *.h ../include
		cp -- *.a ../link
	fi
	)
done;
