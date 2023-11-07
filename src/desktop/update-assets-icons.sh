#!/usr/bin/env bash

set -euo pipefail

cd "$(dirname "$0")"/assets/theme

update_icons() {
	pushd "$1" > /dev/null
	for icon in ${3:-*.svg}; do
		commit=v5.103.0
		dir="$2"
		redir="$icon"
		case "$icon" in
			fa_*|drawpile_*|onion-*|zoom-fit-none.svg)
				continue
				;;
			folder.svg | network-server.svg | network-server-database.svg)
				category=places
				;;
			input-keyboard.svg | monitor.svg | network-modem.svg)
				category=devices
				;;
			dialog-input-devices.svg)
				category=actions
				;;
			audio-volume-high.svg | state-*.svg | dialog-*.svg | security-*.svg | update-none.svg)
				category=status
				;;
			application-exit.svg | edit-delete.svg | im-kick-user.svg | im-ban-kick-user.svg | irc-unvoice.svg)
				# These icons are colored red and so don't exist in a separate
				# dark mode variant. We just have two copies of them though.
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
			window_.svg)
				# Qt will use window.svg as the close button icon, which is just
				# wrong. Hence we stick the underscore at the end to prevent it.
				redir=window.svg
				category=actions
				;;
			*)
				category=actions
				;;
		esac
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
update_icons light icons "$@"
echo "Updating dark icons"
update_icons dark icons-dark "$@"
