#!/bin/sh

echo '##################'
echo '### unit tests ###'
echo '##################'
echo

echo test 1: Bgetre C comments from file
{ ./test_Bgetre_comment | cmp expect_Bgetre_comment - ; } 1>/dev/null 2>error \
	&& echo passed || { echo failed && cat error ; }
echo

echo test 2: Bgetre C functions from file
{ ./test_Bgetre_cfunc | cmp expect_Bgetre_cfunc - ; } 1>/dev/null 2>error \
	&& echo passed || { echo failed && cat error ; }
echo

echo test 3: Bgetre C functions from pipe
{ cat case_Bgetre | ./test_Bgetre_pipe | cmp expect_Bgetre_cfunc - ; } 1>/dev/null 2>error \
	&& echo passed || { echo failed && cat error ; }
echo

rm error
