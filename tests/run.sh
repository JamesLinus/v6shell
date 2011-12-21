#!/bin/sh
#
# Exit status 0 indicates that all tests passed.
# Exit status 1 indicates one or more tests failed.
# Exit status >= 2 indicates some other error.
# -- 
# Jeffrey Allen Neitzel
#

umask 0077
DIR=test_dir
trap 'rtn=$?; rm -rf $DIR; exit $rtn' 0
trap 'exit $?' 1 2 3 15

test X"$0" = X"run.sh" -o X"$0" = X"./run.sh" || {
	echo "Wrong directory" >&2
	exit 2
}
test X"$1" = X"-newlog" && { ACTION=newlog; shift; } || ACTION=runnow
test $# -eq 3 -a \( X"$1" = X"osh" -o X"$1" = X"sh6" \) -a \
     X"$2" = X"if" -a X"$3" = X"goto" || { echo "Usage error" >&2; exit 2; }

# Fail if the binaries are not found or executable.
test -x ../$1 || { echo "$1: not found or not executable" >&2; exit 2; }
test -x ../$2 || { echo "$2: not found or not executable" >&2; exit 2; }
test -x ../$3 || { echo "$3: not found or not executable" >&2; exit 2; }

  SH=../$1
  IF=../$2
GOTO=../$3
ARGS="$SH $IF $GOTO"

#### parameters ####
finput=Input
fcreat=$DIR/Cannot_create
 fopen=$DIR/Cannot_open

pre1=if_test.osh
prelog1=if_test.log
pre2a=goto_test1.osh
pre2b=goto_test2.osh
pre2c=goto_test3.osh

test1=test1.osh		# All command syntax is good.
 log1=test1.log
test2=test2.osh		# All command syntax is bad.
 log2=test2.log
test3=test3.osh		# Test 'cannot open' and 'cannot create' errors.
 log3=test3.log
test4=test4.osh		# Test redirection arguments w/ built-in commands.
test5=test5.osh		# Test 'Command line overflow' errors.
 log5=test5.log
test6=test6.osh		# Test 'Too many characters' errors.
 log6=test6.log
test7=test7.osh		# Test continuation of command lines w/ backslashes.
 log7=test7.log
test8=test8.osh		# Test globbing behaviour.
log8a=test8a.log	# for osh
log8b=test8b.log	# for sh6
if test X"`basename $SH`" = X"osh"; then
	GLOBLOG=$log8a
elif test X"`basename $SH`" = X"sh6"; then
	GLOBLOG=$log8b
fi
####

# See if all the required test files are available; fail if not.
for i in $pre1 $pre2a $pre2b $pre2c \
    $test1 $test2 $test3 $test4 $test5 $test6 $test7 $test8 $finput; do
	test -e "$i" -a -r "$i" || {
		echo "$i: Missing or not readable" >&2; exit 2
	}
done

# Create $DIR and a few temporary test files.
mkdir $DIR || exit 2
touch $fcreat && chmod 0000 $fcreat
test $? -eq 0 || exit 2
touch $fopen && chmod 0000 $fopen
test $? -eq 0 || exit 2

case "$ACTION" in
newlog)
	echo "Generating new reference logs..." >&2
	if rm -f $prelog1 $log1 $log2 $log3 $log5 \
	   $log6 $log7 $log8a $log8b; then
		$SH $pre1 $ARGS >$prelog1 2>&1
		$SH $test1 $ARGS 2>&1 | sed 's/^[0-9][0-9]*/?????/' >$log1
		$SH $test2 $ARGS $DIR >$log2 2>&1
		$SH $test3 $ARGS $DIR >$log3 2>&1
		$SH $test5 $ARGS      >$log5 2>&1
		$SH $test6 $ARGS FOO  >$log6 2>&1
		$SH $test7 $ARGS      >$log7 2>&1
		$SH $test8 $ARGS      >$log8a 2>&1
		../sh6 $test8 ../sh6 $IF $GOTO >$log8b 2>&1

		if chmod 0444 $prelog1 $log1 $log2 $log3 $log5 \
		   $log6 $log7 $log8a $log8b; then
			echo "Done." >&2
			exit 0
		fi
	fi
	echo "Problem generating reference logs." >&2
	exit 2
	;;
runnow)
	# See if all the log files are available; fail if not.
	for i in $prelog1 $log1 $log2 $log3 $log5 $log6 $log7 $log8a $log8b; do
		test -e "$i" -a -r "$i" || {
			echo "$i: Missing or not readable" >&2; exit 2
		}
	done

	# OK, run the tests now.
	bad=0
	echo "If pretest ..." >&2
	$SH $pre1 $ARGS 2>&1 | diff $prelog1 -
	test $? -eq 0 && echo "If pretest passed." >&2 || {
		echo "If pretest failed." >&2
		bad=`expr $bad + 1`
	}

	echo "Goto pretests ..." >&2
	if $SH $pre2a $ARGS &&
	   $SH $pre2b $ARGS 2>&1 | \
		grep '^goto: label: label not found$' >/dev/null &&
	   $SH $pre2c $ARGS 2>&1 | \
		grep '^goto: line 15: label too long$' >/dev/null
	then
		echo "Goto pretests passed." >&2
	else
		echo "Goto pretests failed." >&2
		bad=`expr $bad + 1`
	fi

	echo "Test 1 (takes at least 5 seconds) ..." >&2
	$SH $test1 $ARGS 2>&1 | sed 's/^[0-9][0-9]*/?????/' | diff $log1 -
	test $? -eq 0 && echo "Test 1 passed." >&2 || {
		echo "Test 1 failed." >&2
		bad=`expr $bad + 1`
	}

	echo "Test 2 (may take several seconds) ..." >&2
	$SH $test2 $ARGS $DIR 2>&1 | diff $log2 -
	test $? -eq 0 && echo "Test 2 passed." >&2 || {
		echo "Test 2 failed." >&2
		bad=`expr $bad + 1`
	}

	echo "Test 3 ..." >&2
	$SH $test3 $ARGS $DIR 2>&1 | diff $log3 -
	test $? -eq 0 && echo "Test 3 passed." >&2 || {
		echo "Test 3 failed." >&2
		bad=`expr $bad + 1`
	}

	echo "Test 4 ..." >&2
	$SH $test4 $ARGS $DIR
	test $? -eq 0 && echo "Test 4 passed." >&2 || {
		echo "Test 4 failed." >&2
		bad=`expr $bad + 1`
	}

	echo "Test 5 ..." >&2
	$SH $test5 $ARGS 2>&1 | diff $log5 -
	test $? -eq 0 && echo "Test 5 passed." >&2 || {
		echo "Test 5 failed." >&2
		bad=`expr $bad + 1`
	}

	echo "Test 6 ..." >&2
	$SH $test6 $ARGS FOO 2>&1 | diff $log6 -
	test $? -eq 0 && echo "Test 6 passed." >&2 || {
		echo "Test 6 failed." >&2
		bad=`expr $bad + 1`
	}

	echo "Test 7 ..." >&2
	$SH $test7 $ARGS 2>&1 | diff $log7 -
	test $? -eq 0 && echo "Test 7 passed." >&2 || {
		echo "Test 7 failed." >&2
		bad=`expr $bad + 1`
	}

	echo "Test 8 ..." >&2
	$SH $test8 $ARGS 2>&1 | diff $GLOBLOG -
	test $? -eq 0 && echo "Test 8 passed." >&2 || {
		echo "Test 8 failed." >&2
		bad=`expr $bad + 1`
	}

	if test $bad -gt 0; then
		echo "Total test failures: $bad" >&2
		exit 1
	fi
	echo "All tests passed." >&2
	exit 0
	;;
esac
