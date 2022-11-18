#!/bin/sh

mkdir -p include
mkdir -p link

for l in lib*
do
	cd "$l"
	make
	cp *.h ../include
	cp *.a ../link
	cd ..
done;
