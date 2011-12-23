#!/bin/sh

# Exit status 0 indicates that all tests passed.
# Exit status 1 indicates one or more tests failed.
# Exit status >= 2 indicates some other error.
# -- 
# Jeffrey Allen Neitzel

# Fail if the following binaries are not found in the current directory.
test -x osh  || { echo "osh: not found"  >&2; exit 127; }
test -x if   || { echo "if: not found"   >&2; exit 127; }
test -x goto || { echo "goto: not found" >&2; exit 127; }

#### parameters ####
# Need pathname of current directory.
# Fetch it manually if /bin/sh does not set PWD for us (e.g., NetBSD).
dir=${PWD:-`pwd`}/tests || exit 2

f_input=$dir/Input
f_creat=$dir/Cannot_create
 f_open=$dir/Cannot_open
 f_null=$dir/Link_to_null

pre1=$dir/if_test.osh
pre1ref=$dir/if_test_reference.log
pre2a=$dir/goto_test1.osh
pre2b=$dir/goto_test2.osh
pre2c=$dir/goto_test3.osh

test1=$dir/syngood.osh			# All command syntax is good.
 ref1=$dir/syngood_reference.log	#  *) matching reference log
test2=$dir/synbad.osh			# All command syntax is bad.
 ref2=$dir/synbad_reference.log		#  *) matching reference log
test3=$dir/redirfail.osh		# Test cannot open and cannot create.
 ref3=$dir/redirfail_reference.log	#  *) matching reference log
test4=$dir/redir_sbi.osh		# no reference log used
test5=$dir/redir_subst.osh		# no reference log used
 done=$dir/.check_done			# Touch it if all tests pass.
####

# See if we have all the required files; fail if not.
for i in $pre1 $pre2a $pre2b $pre2c \
    $test1 $test2 $test3 $test4 $test5 $f_input; do
	test -r "$i" || { echo "Missing: $i" >&2; exit 2; }
done

# Create the necessary $f_* files if they don't exist.
test -e "$f_creat" || {
	touch $f_creat && chmod 0000 $f_creat; test $? -eq 0 || exit 2
}
test -e "$f_open"  || {
	touch $f_open && chmod 0000 $f_open; test $? -eq 0 || exit 2
}
test -L "$f_null"  || {
	ln -s /dev/null $f_null; test $? -eq 0 || exit 2
}

# required for testing the osh, if, and goto in '.'
PATH=.:/bin:/usr/bin
export PATH

# Run all tests now.
if test X"$1" = X &&
   test -r "$pre1ref" &&
   test -r "$ref1" && test -r "$ref2" && test -r "$ref3"; then
	bad=0
	echo "If pretest..." >&2
	osh $pre1 2>&1 | diff $pre1ref -
	test $? -eq 0 && echo "If pretest passed." >&2 || {
		echo "If pretest failed." >&2
		bad=`expr $bad + 1`
	}

	echo "Goto pretests..." >&2
	if osh $pre2a &&
	   osh $pre2b 2>&1 | grep 'not found$' >/dev/null &&
	   osh $pre2c 2>&1 | grep 'too long$' >/dev/null; then
		echo "Goto pretests passed." >&2
	else
		echo "Goto pretests failed." >&2
		bad=`expr $bad + 1`
	fi

	echo "Test one..." >&2
	echo "This test will take at least 5 seconds." >&2
	osh $test1 2>&1 | sed 's/^[0-9][0-9]*/?????/' | diff $ref1 -
	test $? -eq 0 && echo "Test one passed." >&2 || {
		echo "Test one failed." >&2
		bad=`expr $bad + 1`
	}

	echo "Test two..." >&2
	osh $test2 2>&1 | diff $ref2 -
	test $? -eq 0 && echo "Test two passed." >&2 || {
		echo "Test two failed." >&2
		bad=`expr $bad + 1`
	}

	echo "Test three..." >&2
	osh $test3 2>&1 | diff $ref3 -
	test $? -eq 0 && echo "Test three passed." >&2 || {
		echo "Test three failed." >&2
		bad=`expr $bad + 1`
	}

	echo "Test four..." >&2
	osh $test4
	test $? -eq 0 && echo "Test four passed." >&2 || {
		echo "Test four failed." >&2
		bad=`expr $bad + 1`
	}

	echo "Test five..." >&2
	osh $test5 $f_input $f_null
	test $? -eq 0 && echo "Test five passed." >&2 || {
		echo "Test five failed." >&2
		bad=`expr $bad + 1`
	}

	if test $bad -gt 0; then
		echo "Total test failures: $bad" >&2
		exit 1
	fi
	echo "All tests passed." >&2
	touch $done
	exit 0
elif test X"$1" = X"-newref"; then # Generate new reference logs.
	echo "Generating new reference logs..." >&2
	if rm -f $pre1ref $done $ref1 $ref2 $ref3; then
		osh $pre1 >$pre1ref 2>&1
		osh $test1 2>&1 | sed 's/^[0-9][0-9]*/?????/' >$ref1
		osh $test2 >$ref2 2>&1
		osh $test3 >$ref3 2>&1

		if chmod 0444 $pre1ref $ref1 $ref2 $ref3; then
			echo "Done." >&2
			exit 0
		fi
	fi
	echo "Problem generating logs." >&2
	exit 2
fi
echo "Missing required reference logs." >&2
exit 2
