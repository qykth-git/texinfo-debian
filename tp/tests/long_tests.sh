#! /bin/sh
# $Id: long_tests.sh,v 1.8 2012/11/13 20:13:47 pertusus Exp $
# Copyright 2010, 2012 Free Software Foundation, Inc.
#
# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.

if [ z"$srcdir" = 'z' ]; then
  srcdir=.
fi

if test "z$LONG_TESTS" != z'yes' && test "z$ALL_TESTS" != z'yes'; then
  echo "Skipping long tests that take a lot of time to run"
  exit 77
fi

if test "z$TEX_HTML_TESTS" = z'yes'; then
  echo "Skipping long tests, only doing HTML TeX tests"
  exit 77
fi

"$srcdir"/parser_tests.sh "$@" \
 sectioning coverage indices nested_formats contents layout
