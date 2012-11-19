#! /bin/sh

# Helper script for tests.

command=$1
if [ $# -gt 0 ]; then 
 shift
 test_name=$1
fi

if [ z"$command" = 'z' ]; then
  echo "Need an arg"
  exit 1
fi

. ./defs || exit 1

if [ "$command" = 'clean' ]; then
  rm -f t/results/*/*.pl.new
  rm -rf t/results/*/*/out_*/
elif [ "$command" = 'generate' ]; then
  for file in t/*.t; do
    $PERL -w $file -g
  done
elif [ "$command" = 'output' ]; then
  for file in t/*.t; do
    $PERL -w $file -o
  done
elif [ "$command" = 'diff' ]; then
  if [ z"$test_name" = 'z' ]; then
    for result in t/results/*/*.pl; do
      diff -a -U 3 $result $result.new
    done
    for result in t/results/*/*/res*/; do
      out=`echo $result | sed 's;res\([^/]*/\)$;out\1;'`
      diff -a -u --exclude=CVS -r $result $out
    done
  else
    for result in t/results/$test_name/*.pl; do
      diff -a -u $result $result.new
    done
    for result in t/results/$test_name/*/res*/; do
      out=`echo $result | sed 's;res\([^/]*/\)$;out\1;'`
      diff -a -u --exclude=CVS -r $result $out
    done
  fi 
elif [ "$command" = 'texis' ]; then
  for file in t/*.t; do
    $PERL -w $file -c
  done
else
  echo "Unknown command (clean|generate|output|diff|texis)"
  exit 1
fi
