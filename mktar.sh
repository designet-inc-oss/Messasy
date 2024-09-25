#!/bin/bash

Version=`grep ^AC_INIT  configure.ac | awk -F, '{print $2}' | tr -d ' '`

make distclean-all
rm -rf messasy-${Version}
mkdir messasy-${Version}
cp -rp * messasy-${Version}
rm -rf  messasy-${Version}/messasy-${Version}
tar cvzf messasy-${Version}.tar.gz messasy-${Version}
rm -rf messasy-${Version}
