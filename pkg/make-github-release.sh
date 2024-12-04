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

    for var in \
            GITHUB_TOKEN TARGET_COMMIT RELEASE_NAME RELEASE_DESCRIPTION_FILE; do
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

run_gh() (
    set -x
    gh --repo "$GIT_REPO_SLUG" "$@"
)

check_args "$@"

release_description="$(cat "$RELEASE_DESCRIPTION_FILE")"
if [[ -z $release_description ]]; then
    note "No release description found in '$RELEASE_DESCRIPTION_FILE'"
    exit 1
fi

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

if [[ $CLOBBER_EXISTING == 'true' ]]; then
    run_gh release delete --cleanup-tag --yes "$RELEASE_NAME" \
        || true
fi

if [[ $PRERELEASE == 'true' ]]; then
    prerelease_arg='--prerelease'
else
    prerelease_arg=
fi

if [[ -n $RELEASE_TITLE ]]; then
    title="$RELEASE_TITLE"
else
    title="$RELEASE_NAME"
fi

run_gh release create \
    --target "$TARGET_COMMIT" \
    --title "$title" \
    --notes "$release_description" \
    --draft \
    $prerelease_arg \
    "$RELEASE_NAME"

for asset in "${assets[@]}"; do
    run_gh release upload "$RELEASE_NAME" "$asset"
done

run_gh release edit "$RELEASE_NAME" --draft=false
