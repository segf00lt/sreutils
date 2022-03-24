#!/bin/sh

for i in libutf libfmt libbio libregexp
do
	cd "$i"
	make
	cp *.h ../../include; cp *.a ../../link
	cd ..
done;
