#! /bin/sh

cat << EOF
#! /bin/sh

case "\$1" in
	hibernate|suspend)
		$1/bin/nitch-notifier
		;;
esac
EOF
