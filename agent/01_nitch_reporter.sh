#! /bin/sh
case "$1" in
	hibernate|suspend)
		/usr/local/bin/nitch-notifier
		;;
esac
