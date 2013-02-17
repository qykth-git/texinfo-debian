#! /bin/sh
# regenerate_cmd_tests.sh: use information from test driving files to
# regenerate test scripts that run only one test, and files lists to be
# use in Makefiles.
# $Id: regenerate_cmd_tests.sh,v 1.1 2013/02/10 15:14:11 pertusus Exp $
# Copyright 2013 Free Software Foundation, Inc.
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# Originally written by Patrice Dumas.

#set -x

test_file='tests-parser.txt'
test_scripts_dir='test_scripts'

test -d $test_scripts_dir || mkdir $test_scripts_dir

dir=`echo $0 | sed 's,/[^/]*$,,'`
outfile=$1
shift

while test z"$1" = 'z-base' -o z"$1" = 'z-long' -o z"$1" = 'z-tex_html'; do
  if test z"$1" = 'z-base'; then
    base_test_dirs=$2
  elif test z"$1" = 'z-long'; then
    long_test_dirs=$2
  elif test z"$1" = 'z-tex_html'; then
    tex_html_test_dirs=$2
  else
    echo "Bad args" 1>&2
    exit 1
  fi
  shift
  shift
done


(
cd "$dir/../tests" || exit 1

test_driving_files='test_driving_files_generated_list ='
one_test_files='one_test_files_generated_list = '

gather_tests() {
type=$1
shift
test_dirs=$1
for test_dir in $test_dirs; do
  driving_file=$test_dir/tests-parser.txt
  if test -f $driving_file; then
    test_driving_files="$test_driving_files $driving_file"
    while read line
    do
    if echo $line | grep -qs '^ *#'; then continue; fi
# there are better ways
    name=`echo $line | awk '{print $1}'`
    arg=$name
    file=`echo $line | awk '{print $2}'`
    remaining=`echo $line | sed 's/[a-zA-Z0-9_./-]*  *[a-zA-Z0-9_./-]* *//'`
    test "z$name" = 'z' -o "$zfile" = 'z' && continue
    basename=`basename $file .texi`
    if test "z$name" = 'ztexi' ; then
      name="texi_${basename}"
      arg="texi ${basename}.texi"
    fi
    one_test_file="$test_scripts_dir/${test_dir}_$name.sh"
    one_test_files="$one_test_files \\
     $one_test_file"
    echo '#! /bin/sh

if test z"$srcdir" = "z"; then
  srcdir=.
fi

command=run_parser_all.sh
one_test_logs_dir=test_log
diffs_dir=diffs

' > $one_test_file
    if test $type = 'base'; then
      echo '
if test "z$LONG_TESTS" = z"yes"; then
  echo "Skipping short tests because we are only doing long tests"
  exit 77
fi' >> $one_test_file
    elif test $type = 'long'; then
      echo '
if test "z$LONG_TESTS" != z"yes" && test "z$ALL_TESTS" != z"yes"; then
  echo "Skipping long tests that take a lot of time to run"
  exit 77
fi

if test "z$TEX_HTML_TESTS" = z"yes"; then
  echo "Skipping long tests, only doing HTML TeX tests"
  exit 77
fi
' >> $one_test_file
    elif test $type = 'tex_html'; then
      echo '
if test "z$TEX_HTML_TESTS" != z"yes"; then
  echo "Skipping HTML TeX tests that are not easily reproducible"
  exit 77
fi
' >> $one_test_file
    fi
    echo "dir=$test_dir
arg='$arg'
name='$name'
"'[ -d "$dir" ] || mkdir $dir

srcdir_test=$dir; export srcdir_test;
cd "$dir" || exit 99
../"$srcdir"/"$command" -dir $dir $arg
exit_status=$?
cat $one_test_logs_dir/$name.log
if test -f $diffs_dir/$name.diff; then
  echo 
  cat $diffs_dir/$name.diff
fi
exit $exit_status
' >> $one_test_file
    chmod 0755 $one_test_file
    done < $driving_file
  else
    echo "Missing file $driving_file" 1>&2
    exit 1
  fi
done
}

basefile=`basename $outfile`
cat >$outfile <<END_HEADER
# $basefile generated by $0.
#
# Copyright 2013 Free Software Foundation, Inc.
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

END_HEADER

gather_tests long "$long_test_dirs"
gather_tests base "$base_test_dirs"
gather_tests tex_html "$tex_html_test_dirs"

echo "$test_driving_files
" >> $outfile

echo "$one_test_files
" >>$outfile

)
