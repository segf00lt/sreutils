#!/bin/sh

mkdir -p include
mkdir -p link

for l in lib*
do
	(
	[ "$l" == 'libsre' ] && continue
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

cd libsre
if [ "$1" = 'clean' ]; then
	make clean
else
	make
	cp -- *.h ../include
	cp -- *.a ../link
fi
cd ..
