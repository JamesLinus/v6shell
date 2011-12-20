#!/bin/sh -
#
# Exit status == 0 when all tests pass.
# Exit status == 1 when one or more tests fail.
# Exit status >= 2 when an error occurs.
# -- 
# Jeffrey Allen Neitzel
#

if test `id -u` -eq 0; then
	echo "Cannot run tests as the superuser" >&2
	exit 2
fi
if test X"$0" != X"run.sh" -a X"$0" != X"./run.sh"; then
	echo "Wrong directory" >&2
	exit 2
fi
if test X"$1" = X"-newlog"; then
	ACTION=newlog
	shift
else
	ACTION=run
fi
if test $# -ne 1 -o \( X"$1" != X"osh" -a X"$1" != X"sh6" \); then
	echo "Usage error" >&2
	exit 2
fi

# Fail if the binaries are not found or executable.
test -x ../$1   || { echo   "$1: Missing or not executable" >&2; exit 2; }
test -x ../if   || { echo   "if: Missing or not executable" >&2; exit 2; }
test -x ../goto || { echo "goto: Missing or not executable" >&2; exit 2; }

  SH=../$1
  IF=../if
GOTO=../goto
ARGS="$SH $IF $GOTO"

umask 0077
DIR=test_dir
trap 'rtn=$?; rm -rf $DIR; exit $rtn' EXIT
trap 'exit $?' HUP INT QUIT TERM

#### parameters ####
finput=Input
 fnul1=nuls1.osh
 fnul2=nuls2.osh
 fsyn1=syntax_error.osh.in
 fsyn2=syntax.list
fcreat=$DIR/Cannot_create
 fopen=$DIR/Cannot_open

 test1=test01.osh	# Test goto(1) behaviour.
  log1=test01.log
 test2=test02.osh	# Test if(1) behaviour.
  log2=test02.log
 test3=test03.osh	# Test good shell syntax.
  log3=test03.log
 test4=test04.osh	# Test bad shell syntax.
  log4=test04.log
 test5=test05.osh	# Test shell redirection behaviour.
  log5=test05.log
 test6=test06.osh	# Test shell command-line overflow.
  log6=test06.log
 test7=test07.osh	# Test shell too many characters.
  log7=test07.log
 test8=test08.osh	# Test shell too many args.
  log8=test08.log
 test9=test09.osh	# Test shell `\' line continuation.
  log9=test09.log
test10=test10.osh	# Test shell globbing behaviour.
 log10=test10.log
####

# See if all the required test files are available; fail if not.
for i in $finput $fnul1 $fnul2 $fsyn1 $fsyn2 \
    $test1 $test2 $test3 $test4 $test5 $test6 $test7 $test8 $test9 $test10; do
	test -e "$i" -a -r "$i" || {
		echo "$i: Missing or not readable" >&2; exit 2
	}
done

# Create the needed test directory and files.
mkdir $DIR || exit 2
touch $fcreat && chmod 0000 $fcreat || exit 2
touch $fopen  && chmod 0000 $fopen  || exit 2

case "$ACTION" in
newlog)
	echo "Generating new reference logs..." >&2
	if rm -f $log1 $log2 $log3 $log4 \
	   $log5 $log6 $log7 $log8 $log9 $log10; then
		$SH $test1  $ARGS >$log1 2>&1
		$SH $test2  $ARGS $DIR FOO >$log2 2>&1
		$SH $test3  $ARGS 2>&1 | sed 's/^[0-9][0-9]*/?????/' >$log3
		$SH $test4  $ARGS $DIR >$log4 2>&1
		$SH $test5  $ARGS $DIR >$log5 2>&1
		$SH $test6  $ARGS      >$log6 2>&1
		$SH $test7  $ARGS FOO  >$log7 2>&1
		$SH $test8  $ARGS      >$log8 2>&1
		$SH $test9  $ARGS      >$log9 2>&1
		$SH $test10 $ARGS      >$log10 2>&1

		if chmod 0444 $log1 $log2 $log3 $log4 \
		   $log5 $log6 $log7 $log8 $log9 $log10; then
			echo "Done." >&2
			exit 0
		fi
	fi
	echo "Problem generating reference logs." >&2
	exit 2
	;;
run)
	# See if all the log files are available; fail if not.
	for i in $log1 $log2 $log3 $log4 \
	    $log5 $log6 $log7 $log8 $log9 $log10; do
		test -e "$i" -a -r "$i" || {
			echo "$i: Missing or not readable" >&2; exit 2
		}
	done

	# OK, run the tests now.
	bad=0
	echo "Testing $1(1), if(1), and goto(1):" >&2
	echo "-------------------------------------------" >&2
	echo "Test  1:  goto(1)                       ..." >&2
	$SH $test1 $ARGS 2>&1 | diff -u $log1 -
	test $? -eq 0&&echo "Test  1:  passed                          ." >&2||{
		echo "Test  1:  failed                          ." >&2
		bad=`expr $bad + 1`
	}

	echo "Test  2:  if(1)                         ..." >&2
	$SH $test2 $ARGS $DIR FOO 2>&1 | diff -u $log2 -
	test $? -eq 0&&echo "Test  2:  passed                          ." >&2||{
		echo "Test  2:  failed                          ." >&2
		bad=`expr $bad + 1`
	}

	echo "Test  3:  good $1(1) syntax            ..." >&2
	$SH $test3 $ARGS 2>&1 | sed 's/^[0-9][0-9]*/?????/' | diff -u $log3 -
	test $? -eq 0&&echo "Test  3:  passed                          ." >&2||{
		echo "Test  3:  failed                          ." >&2
		bad=`expr $bad + 1`
	}

	echo "Test  4:  bad $1(1) syntax             ..." >&2
	$SH $test4 $ARGS $DIR 2>&1 | diff -u $log4 -
	test $? -eq 0&&echo "Test  4:  passed                          ." >&2||{
		echo "Test  4:  failed                          ." >&2
		bad=`expr $bad + 1`
	}

	echo "Test  5:  $1(1) redirection            ..." >&2
	$SH $test5 $ARGS $DIR 2>&1 | diff -u $log5 -
	test $? -eq 0&&echo "Test  5:  passed                          ." >&2||{
		echo "Test  5:  failed                          ." >&2
		bad=`expr $bad + 1`
	}

	echo "Test  6:  $1(1) command-line overflow  ..." >&2
	$SH $test6 $ARGS 2>&1 | diff -u $log6 -
	test $? -eq 0&&echo "Test  6:  passed                          ." >&2||{
		echo "Test  6:  failed                          ." >&2
		bad=`expr $bad + 1`
	}

	echo "Test  7:  $1(1) too many characters    ..." >&2
	$SH $test7 $ARGS FOO 2>&1 | diff -u $log7 -
	test $? -eq 0&&echo "Test  7:  passed                          ." >&2||{
		echo "Test  7:  failed                          ." >&2
		bad=`expr $bad + 1`
	}

	echo "Test  8:  $1(1) too many args          ..." >&2
	$SH $test8 $ARGS 2>&1 | diff -u $log8 -
	test $? -eq 0&&echo "Test  8:  passed                          ." >&2||{
		echo "Test  8:  failed                          ." >&2
		bad=`expr $bad + 1`
	}

	echo "Test  9:  $1(1) \`\' line continuation  ..." >&2
	$SH $test9 $ARGS 2>&1 | diff -u $log9 -
	test $? -eq 0&&echo "Test  9:  passed                          ." >&2||{
		echo "Test  9:  failed                          ." >&2
		bad=`expr $bad + 1`
	}

	echo "Test 10:  $1(1) globbing               ..." >&2
	$SH $test10 $ARGS 2>&1 | diff -u $log10 -
	test $? -eq 0&&echo "Test 10:  passed                          ." >&2||{
		echo "Test 10:  failed                          ." >&2
		bad=`expr $bad + 1`
	}
	echo "-------------------------------------------" >&2

	if test $bad -gt 0; then
		echo "Total test failures: $bad" >&2
		exit 1
	fi
	echo "All tests passed." >&2
	exit 0
	;;
esac
