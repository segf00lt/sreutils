#!/bin/sh

[[ ! -d bin ]] && mkdir bin
[[ -x siv_debug ]] && mv siv_debug bin

FLAGS='-Wall -Wpedantic -g -O0 -fsanitize=address'
INCLUDEPATH='../lib/include'
LINKPATH='../lib/link'

testbin=()

function unit {
	run=''
	check=''

	[[ -z "$2" ]] && run="./$1.bin" || run="$2" # if no $2 run ./$1
	[[ -z "$3" ]] && check="expect/$1" || check="$3"

	printf "test: $1\nstatus: "
	{ eval "$run" | cmp "$check" - ; } 1>/dev/null 2>err && echo passed || { echo failed && cat err ; }
	echo
}

for src in *.c
do
	bin=${src%.c}.bin
	gcc $FLAGS $src -o $bin -I$INCLUDEPATH -L$LINKPATH -l'sre' -l'bio' -l'regexp9' -l'fmt' -l'utf'
	testbin+=($bin)
done

printf '\n##################\n### unit tests ###\n##################\n\n'

unit Bgetre_comment
unit Bgetre_cfunc
unit Bgetre_pipe 'cat case/Bgetre | ./Bgetre_pipe.bin' 'expect/Bgetre_cfunc'
unit strgetretest
unit strgetre_cascade_1
unit strgetre_cascade_2
unit sivfunctest
unit Bgetre_greedy
unit strgetre_greedy
unit siv_comment
unit siv_comment_greedy
unit siv_cfunc
unit greedy_default
unit siv_recurse_directory
unit siv_singledot
unit siv_singledot_locat

rm -f err
mv -t bin ${testbin[@]}
