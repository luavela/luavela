#!/bin/sh
set -eu

TOOL=$(basename "$0")
HELP=$(cat <<HELP
${TOOL} - Just bumps copyright year all over the sources.
          It ain't much but it's honest work.

    --

    Soon her eye fell upon a little glass box lying underneath
    the table. She opened it and found in it a very small
    cake, on which the words "EAT ME" were beautifully marked
    in currants.

    - Alice's Adventures in Wonderland.
HELP
)

# Parse CLI options.
OPTIONS=$(getopt -o pqh -n "$TOOL" -- "$@")
eval set -- "$OPTIONS"
while true; do
	case "$1" in
		--) shift; break;;
		-p) PRETEND=1; shift;;
		-h) printf "%s\n" "${HELP}";
			exit 0;;
	esac
done

PRETEND=${PRETEND:-}

# Do not proceed if there are some CLI arguments left. Everything
# should be parsed before this line. Refer to the help message in
# a funny way.
if [ $# -ne 0 ]; then
	echo "RUN ME"
	exit 1;
fi

overwrite() {
	# Check if we're running in term.
	if [ -t 1 ]; then
		echo -e "\r\033[1A\033[0K$@"
	else
		echo -e "$@"
	fi
}

makereplace() {
	# XXX: This function uses "upvalues" ($CURYEAR and
	# $NEWYEAR) to make the function more convenient in use.
	# Hence, do not call this function prior to these upvalues
	# definition.
	if [ -z $CURYEAR -o -z $NEWYEAR ]; then
		echo "FATAL: Either CURYEAR or NEWYEAR are undefined" && exit 1
	fi
	REGEXP="$(printf "$1" $CURYEAR)"
	REPLACEMENT="$(printf "$1" $NEWYEAR)"
	echo -E "s/$REGEXP/$REPLACEMENT/"
}

# 0. Greeting.
TODAY=$(date --date='today' --iso-8601)
echo "Hello! Today is $TODAY. Let's do some janitorial maintenance!"
NEWYEAR=$(date +"%Y")
CURYEAR=$(($NEWYEAR-1))
echo "* Current year is $CURYEAR. Bumping to $NEWYEAR."
echo "** Starting"

GITROOT=$(git rev-parse --show-toplevel)

overwrite "** Completed: 0%"

# 1.1. Update copyright notice **almost** everywhere...
PATTERN=$(makereplace 'Copyright (C) 2020-%d LuaVela Authors.')
find $GITROOT -type f | xargs -I{} sed -i "$PATTERN" {}

overwrite "** Completed: 99%"

# 1.2. ...except docs/conf.py due to another pattern...
PATTERN=$(makereplace '2020-%d LuaVela Authors,')
sed -i "$PATTERN" "$GITROOT/docs/conf.py"

overwrite "** Completed: 100%"

# 1.3. ...and COPYRIGHT file for the same reason.
PATTERN=$(makereplace 'Copyright 2020-%d LuaVela Authors.')
sed -i "$PATTERN" "$GITROOT/COPYRIGHT"

overwrite "** Completed!"


# 2. Commit everything, if not pretend.
if [ $PRETEND ]; then
	cat <<PRETEND
You're almost done: changes are applied, but not committed.
PRETEND
	exit 0
fi

echo "* Commit the changes..."
if ! git branch --show-current | grep 'bump-copyright-year$' >/dev/null ; then
	cat <<WRONGBRANCH
Oops, looks like you're commiting to the wrong branch. I hope not
to the master one. It's better to checkout somewhere else like this:
>>> git checkout -b ${USER}/bump-copyright-year
WRONGBRANCH
	exit 1
fi;
git commit -a -s -m 'Bump copyright date'
echo "You're all set, just push it to the server! Happy New Year :)"
