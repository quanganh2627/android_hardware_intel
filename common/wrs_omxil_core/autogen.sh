#! /bin/sh
AUTORECONF_TOOL=`which autoreconf`
if test -z $AUTORECONF_TOOL; then
	echo "no autoreconf, check your autotools installation"
	exit 1
else
	autoreconf -ivf || exit $?
fi
