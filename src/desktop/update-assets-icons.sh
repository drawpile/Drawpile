#!/usr/bin/env bash

set -euo pipefail

cd "$(dirname "$0")"/assets/theme

update_icons() {
	pushd "$1" > /dev/null
	for icon in *.svg; do
		commit=v5.103.0
		dir="$2"
		case "$icon" in
			drawpile_*)
				continue
				;;
			folder.svg)
				category=places
				;;
			dialog-*.svg | security-*.svg | update-none.svg)
				category=status
				;;
			application-exit.svg | edit-delete.svg | im-kick-user.svg | irc-unvoice.svg)
				dir=icons
				category=actions
				;;
			media-record.svg)
				# This icon design was changed in commit
				# 83c8768b2a7022cd1119167fd6b17c68cf264bc6 to be a giant red
				# circle instead of a red dot with a circle, so use the one from
				# the version before that
				commit=v5.63.0
				category=actions
				;;
			*)
				category=actions
				;;
		esac
		redir="$icon"
		while true; do
			url="https://raw.githubusercontent.com/KDE/breeze-icons/$commit/$dir/$category/22/$redir"
			(curl -f -s -S -L "$url" | sed -e $'s/\r//g' > "$icon") || (echo "$url failed" && false)
			chmod 644 "$icon"
			redir=$(head -n 1 "$icon")
			[[ "$redir" =~ \.svg$ ]] || break
		done
		echo "$url => $icon"
	done
	popd > /dev/null
}

echo "Updating light icons"
update_icons light icons
echo "Updating dark icons"
update_icons dark icons-dark
