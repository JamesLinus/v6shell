#!/bin/sh
#
# Exit status 0 indicates that all tests passed.
# Exit status 1 indicates one or more tests failed.
# Exit status >= 2 indicates some other error.
# -- 
# Jeffrey Allen Neitzel
#

NEWREF=false
if test X"$1" = X"-newref"; then
	NEWREF=true
	shift
fi
test $# -eq 3 || exit 2

# Fail if the binaries are not found or executable.
test -x ./$1 || { echo "./$1: not found or not executable" >&2; exit 2; }
test -x ./$2 || { echo "./$2: not found or not executable" >&2; exit 2; }
test -x ./$3 || { echo "./$3: not found or not executable" >&2; exit 2; }

  SH=./$1
  IF=./$2
GOTO=./$3

#### parameters ####
# Set the absolute pathname of the test directory.
dir=${PWD:-`pwd`}/tests || exit 2

finput=$dir/Input
fcreat=$dir/Cannot_create
 fopen=$dir/Cannot_open

pre1=$dir/if_test.osh
prelog1=$dir/if_test.log
pre2a=$dir/goto_test1.osh
pre2b=$dir/goto_test2.osh
pre2c=$dir/goto_test3.osh

test1=$dir/test1.osh	# All command syntax is good.
 log1=$dir/test1.log
test2=$dir/test2.osh	# All command syntax is bad.
 log2=$dir/test2.log
test3=$dir/test3.osh	# Test cannot open and cannot create.
 log3=$dir/test3.log
test4=$dir/test4.osh	# Test 'Command line overflow' errors.
 log4=$dir/test4.log
test5=$dir/test5.osh	# Test 'Too many characters' errors.
 log5=$dir/test5.log
test6=$dir/test6.osh	# Test redirection arguments w/ built-in commands.
####

# See if we have all the required test files; fail if not.
for i in $pre1 $pre2a $pre2b $pre2c \
    $test1 $test2 $test3 $test4 $test5 $test6 $finput; do
	test -r "$i" || { echo "Missing: $i" >&2; exit 2; }
done

# Create the necessary $f* files if they don't exist.
test -e "$fcreat" || {
	touch $fcreat && chmod 0000 $fcreat; test $? -eq 0 || exit 2
}
test -e "$fopen"  || {
	touch $fopen && chmod 0000 $fopen; test $? -eq 0 || exit 2
}

# Run all tests now.
if ! $NEWREF &&
   test -r "$prelog1" &&
   test -r "$log1" && test -r "$log2" && test -r "$log3" &&
   test -r "$log4" && test -r "$log5"; then
	echo "Testing $SH, $IF, and $GOTO now..." >&2
	bad=0
	echo "If pretest..." >&2
	$SH $pre1 $SH $IF $GOTO 2>&1 | diff $prelog1 -
	test $? -eq 0 && echo "If pretest passed." >&2 || {
		echo "If pretest failed." >&2
		bad=`expr $bad + 1`
	}

	echo "Goto pretests..." >&2
	if $SH $pre2a $SH $IF $GOTO &&
	   $SH $pre2b $SH $IF $GOTO 2>&1 | \
		grep '^goto: label: label not found$' >/dev/null &&
	   $SH $pre2c $SH $IF $GOTO 2>&1 | \
		grep '^goto: line 15: label too long$' >/dev/null
	then
		echo "Goto pretests passed." >&2
	else
		echo "Goto pretests failed." >&2
		bad=`expr $bad + 1`
	fi

	echo "Test one..." >&2
	echo "This test will take at least 5 seconds." >&2
	$SH $test1 2>&1 | sed 's/^[0-9][0-9]*/?????/' | diff $log1 -
	test $? -eq 0 && echo "Test one passed." >&2 || {
		echo "Test one failed." >&2
		bad=`expr $bad + 1`
	}

	echo "Test two..." >&2
	$SH $test2 $SH $IF 2>&1 | diff $log2 -
	test $? -eq 0 && echo "Test two passed." >&2 || {
		echo "Test two failed." >&2
		bad=`expr $bad + 1`
	}

	echo "Test three..." >&2
	$SH $test3 $SH $IF 2>&1 | diff $log3 -
	test $? -eq 0 && echo "Test three passed." >&2 || {
		echo "Test three failed." >&2
		bad=`expr $bad + 1`
	}

	echo "Test four..." >&2
	$SH $test4 $SH $IF $GOTO 2>&1 | diff $log4 -
	test $? -eq 0 && echo "Test four passed." >&2 || {
		echo "Test four failed." >&2
		bad=`expr $bad + 1`
	}

	echo "Test five..." >&2
	$SH $test5 $SH $IF $GOTO FOO 2>&1 | diff $log5 -
	test $? -eq 0 && echo "Test five passed." >&2 || {
		echo "Test five failed." >&2
		bad=`expr $bad + 1`
	}

	echo "Test six..." >&2
	$SH $test6
	test $? -eq 0 && echo "Test six passed." >&2 || {
		echo "Test six failed." >&2
		bad=`expr $bad + 1`
	}

	if test $bad -gt 0; then
		echo "Total test failures: $bad" >&2
		exit 1
	fi
	echo "All tests passed." >&2
	exit 0
elif $NEWREF; then # Generate new reference logs.
	echo "Generating new reference logs..." >&2
	if rm -f $prelog1 $log1 $log2 $log3 $log4 $log5; then
		$SH $pre1 $SH $IF $GOTO >$prelog1 2>&1
		$SH $test1 2>&1 | sed 's/^[0-9][0-9]*/?????/' >$log1
		$SH $test2 $SH $IF >$log2 2>&1
		$SH $test3 $SH $IF >$log3 2>&1
		$SH $test4 $SH $IF $GOTO >$log4 2>&1
		$SH $test5 $SH $IF $GOTO FOO >$log5 2>&1

		if chmod 0444 $prelog1 $log1 $log2 $log3 $log4 $log5; then
			echo "Done." >&2
			exit 0
		fi
	fi
	echo "Problem generating reference logs." >&2
	exit 2
fi
echo "Missing reference log(s)." >&2
exit 2
