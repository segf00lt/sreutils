#!/bin/sh

for l in lib*
do
	cd "$l"
	make
	cp *.h ../include
	cp *.a ../link
	cd ..
done;
