#!/bin/sh

FLAGS='-Wall -Wpedantic -g -O0 -fsanitize=address'
INCLUDEPATH='../lib/include'
LINKPATH='../lib/link'

testbin=()

for src in *.c
do
	bin=${src%.c}.bin
	gcc $FLAGS $src ../structregex.c -o $bin -I$INCLUDEPATH -L$LINKPATH -l'bio' -l'regexp9' -l'fmt' -l'utf'
	testbin+=($bin)
done

printf '\n##################\n### unit tests ###\n##################\n\n'

printf 'test: Bgetre_comment\nstatus: '
{ ./Bgetre_comment.bin | cmp expect/Bgetre_comment - ; } 1>/dev/null 2>error \
	&& echo passed || { echo failed && cat error ; }
echo

printf 'test: Bgetre_cfunc\nstatus: '
{ ./Bgetre_cfunc.bin | cmp expect/Bgetre_cfunc - ; } 1>/dev/null 2>error \
	&& echo passed || { echo failed && cat error ; }
echo

printf 'test: Bgetre_pipe\nstatus: '
{ cat 'case/Bgetre' | ./Bgetre_pipe.bin | cmp expect/Bgetre_cfunc - ; } 1>/dev/null 2>error \
	&& echo passed || { echo failed && cat error ; }
echo

rm -f error
rm -f ${testbin[@]}
