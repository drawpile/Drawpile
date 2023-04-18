#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -ueo pipefail
set +x

note() {
    echo "$@" 1>&2
}

check_args() {
    local error fields var
    error=0

    if [[ $# -eq 0 ]]; then
        note 'No assets to upload given'
        error=1
    else
        while [[ $# -ne 0 ]]; do
            IFS=';' read -ra fields <<< "$1"
            if [[ ${#fields[@]} -ne 3 ]]; then
                note "Invalid argument '$1'"
                error=1
            fi
            shift
        done
    fi

    if [[ $error -ne 0 ]]; then
        note
        note 'Each argument must be a triple of the form: dir;pattern;label'
        note 'dir: The directory that the asset is found in.'
        note 'pattern: A "find" pattern to get the file in "dir".'
        note 'label: What to call this asset when uploaded to the release.'
        note
    fi

    for var in GITHUB_TOKEN TARGET_COMMIT RELEASE_NAME RELEASE_DESCRIPTION; do
        if [[ -z "${!var:-}" ]]; then
            note "Environment variable '$var' must be set"
            error=1
        fi
    done

    if [[ $error -ne 0 ]]; then
        note
        note 'Invalid arguments and/or environment variables given, exiting'
        exit 2
    fi
}

check_args "$@"

GIT_REPO_SLUG="${GIT_REPO_SLUG:-drawpile/Drawpile}"

assets=()
error=0
while [[ $# -ne 0 ]]; do
    IFS=';' read -ra fields <<< "$1"
    shift

    dir="${fields[0]}"
    pattern="${fields[1]}"
    path="$(find "$dir" -name "$pattern")"
    count="$(echo "$path" | wc -l)"
    if [[ $count -ne 1 ]]; then
        note "$dir: expected 1 path matching '$pattern', but got $count"
        error=1
    fi

    assets+=("$path#${fields[2]}")
done

if [[ $error -ne 0 ]]; then
    note 'Error(s) collecting release assets'
    exit 1
fi

gh release delete --repo "$GIT_REPO_SLUG" --cleanup-tag --yes "$RELEASE_NAME" \
    || true

gh release create \
    --repo "$GIT_REPO_SLUG" \
    --prerelease \
    --target "$TARGET_COMMIT" \
    --title "$RELEASE_NAME" \
    --notes "$RELEASE_DESCRIPTION" \
    "$RELEASE_NAME" "${assets[@]}"
