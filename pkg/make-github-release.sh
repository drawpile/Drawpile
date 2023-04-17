#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -ueo pipefail
set +x

note() {
    echo "$@" 1>&2
}

die() {
    note "$@"
    exit 1
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

BASE_URL="${BASE_URL:-https://api.github.com}"
GIT_REPO_SLUG="${GIT_REPO_SLUG:-drawpile/Drawpile}"

prepare_assets() {
    local dir pattern path count
    while [[ $# -ne 0 ]]; do
        IFS=';' read -ra fields <<< "$1"
        shift

        dir="${fields[0]}"
        pattern="${fields[1]}"
        path="$(find "$dir" -name "$pattern")"
        count="$(echo "$path" | wc -l)"
        if [[ $count -ne 1 ]]; then
            die "$dir: expected 1 path matching '$pattern', but got $count"
        fi

        # Write out path and label pairs, later processed by upload_assets.
        echo "$path"
        echo "${fields[2]}"
    done
}

api_call() {
    local method url
    method="$1"
    url="$2"
    shift 2
    curl --no-progress-meter -L -X "$method" \
        -H "Accept: application/vnd.github+json" \
        -H "Authorization: token $GITHUB_TOKEN"\
        -H "X-GitHub-Api-Version: 2022-11-28" \
        "$@" "$url" | jq -MS
}

get_existing_release() {
    note "Get release '$RELEASE_NAME'"
    api_call GET "$BASE_URL/repos/$GIT_REPO_SLUG/releases/tags/$RELEASE_NAME"
}

delete_existing_release() {
    local release_id
    release_id="$1"
    note "Delete release with id '$release_id'"
    api_call DELETE "$BASE_URL/repos/$GIT_REPO_SLUG/releases/$release_id"
}

delete_existing_tag() {
    note "Delete tag '$RELEASE_NAME'"
    api_call DELETE "$BASE_URL/repos/$GIT_REPO_SLUG/git/refs/tags/$RELEASE_NAME"
}

create_release() {
    local json
    note "Create release '$RELEASE_NAME' on commit '$TARGET_COMMIT'"
    json="$(jq -n \
        --arg name "$RELEASE_NAME" \
        --arg commit "$TARGET_COMMIT" \
        --arg body "$RELEASE_DESCRIPTION" \
        '{
            "body": $body,
            "draft": true,
            "name": $name,
            "prerelease": true,
            "tag_name": $name,
            "target_commitish": $commit
        }')"
    api_call POST "$BASE_URL/repos/$GIT_REPO_SLUG/releases" -d "$json"
}

urlencode() {
    printf '%s' "$1" | jq -sRr '@uri'
}

create_release_asset() {
    local url path label name
    url="$1"
    path="$2"
    label="$3"
    name="$(basename "$path")"
    note "Create release asset named '$name' labeled '$label' from '$path'"
    api_call POST "$url?name=$(urlencode "$name")&label=$(urlencode "$label")" \
        -H "Content-Type: application/octet-stream" \
        --data-binary "@$path"
}

upload_assets() {
    local url
    url="$1"
    # Read two lines at a time, first line is the path, second is the label.
    while mapfile -t -n 2 pair && [[ ${#pair[@]} -ne 0 ]]; do
        create_release_asset "$url" "${pair[0]}" "${pair[1]}"
    done
}

publish_release() {
    local release_id
    release_id="$1"
    note "Publish release with id '$release_id'"
    api_call PATCH "$BASE_URL/repos/$GIT_REPO_SLUG/releases/$release_id" \
        -d '{"draft":false}'
}

# Check if all assets are present. The result will be pairs of lines, the first
# line in each pair is the path, the second is the label.
assets_to_upload="$(prepare_assets "$@")"

# If we already have a release of the same name, we replace it.
existing_release="$(get_existing_release)"
if echo "$existing_release" | jq -e .id >/dev/null; then
    existing_release_id="$(echo "$existing_release" | jq -r .id)"
    delete_existing_release "$existing_release_id"
fi

# Don't bother to check if the tags exists, just try to delete it.
delete_existing_tag

# Create the release in draft mode.
release="$(create_release)"
if echo "$release" | jq -e .upload_url >/dev/null; then
    # The release contains a URL to use for uploading assets to, but it has some
    # some hypermedia "garbage" in braces at the end that we need to strip off.
    upload_url="$(echo "$release" | jq -r .upload_url | perl -pe 's/\{.*\}$//')"
    echo "$assets_to_upload" | upload_assets "$upload_url"
    # Switch the release over from draft to published.
    release_id="$(echo "$release" | jq -r .id)"
    publish_release "$release_id"
else
    die 'No upload_url in release response, release creation probably failed'
fi
