#!/usr/bin/env bash

set -ueo pipefail

DEFAULT_REPO=drawpile/Drawpile

usage() {
	echo "Usage: $0 [branch] [version]"
	echo
	echo "Branch defaults to 'master'."
	echo "Version defaults to what is listed in Cargo.toml in the branch."
	echo "Version should only be specified for pre-releases."
	echo
	echo "Supported environment variables:"
	echo "  REPO: The GitHub repository (user/repo) to use for the release."
	echo "        Defaults to $DEFAULT_REPO."
	exit 0
}

set_package_version() {
	sed -i '' -e "s/^\\(version[[:space:]]*=[[:space:]]*\\)\"[^\"]*\"/\\1\"$1\"/" Cargo.toml
}

if [ "$1" == "--help" ]; then
	usage
elif [ "$1" == "" ]; then
	BRANCH="master"
else
	BRANCH=$1
fi

if [ "$2" != "" ]; then
	VERSION=$2
fi

REPO=${REPO:-$DEFAULT_REPO}
ROOT_DIR=$(cd "$(dirname "$0")"/.. && pwd)
BUILD_DIR="$ROOT_DIR/build-release"

if [ -d "$BUILD_DIR" ]; then
	echo "Existing build directory detected at $BUILD_DIR"
	echo "Aborted."
	exit 1
fi

echo "This is an internal Drawpile release script!"
echo -n "Press 'y' to create a new Drawpile release from $REPO branch $BRANCH"
if [ "$VERSION" == "" ]; then
	echo "."
else
	echo -e "\nwith version override $VERSION."
fi
echo "(You can abort pushing upstream later on if something goes wrong.)"
read -r -s -n 1

if [ "$REPLY" != "y" ]; then
	echo "Aborted."
	exit 0
fi

# Using a pristine copy of the repo for the release to avoid local changes
# making their way into the release process, and to avoid polluting the local
# repo with wrong changes if the release process is aborted
cd "$ROOT_DIR"
mkdir "$BUILD_DIR"
git clone --recursive "git@github.com:$REPO" "$BUILD_DIR"

cd "$BUILD_DIR"

# Newly created tags and updated branches are stored so they can be pushed all
# at once at the end after the other work is guaranteed to be successful
PUSH_BRANCHES="$BRANCH"

echo -e "\nBuilding $BRANCH branch...\n"

git checkout "$BRANCH"

# head is needed in case the version number was manually updated to something
# which has a tag with a number in it, otherwise there will be multiple matches
# on the line instead of just the first one
VERSION_CARGO_NO_TAGS=$(grep -o '^version[[:space:]]*=[[:space:]]*"[^"]*"' Cargo.toml | grep -o "[0-9][0-9.]*" | head -1)

if [ "$VERSION" == "" ]; then
	VERSION=$VERSION_CARGO_NO_TAGS

	# Converting the version number to an array to auto-generate the next
	# release version number
	OLDIFS=$IFS
	IFS="."
	PRE_VERSION_SPLIT=("$VERSION")
	IFS=$OLDIFS

	if [[ $VERSION =~ \.0$ ]]; then
		MAKE_BRANCH="${PRE_VERSION_SPLIT[0]}.${PRE_VERSION_SPLIT[1]}"
		BRANCH_VERSION="${PRE_VERSION_SPLIT[0]}.${PRE_VERSION_SPLIT[1]}.$((PRE_VERSION_SPLIT[2] + 1))-pre"

		# The next release is usually going to be a minor release; if the next
		# version is to be a major release, the package version in Git will need
		# to be manually updated
		PRE_VERSION="${PRE_VERSION_SPLIT[0]}.$((PRE_VERSION_SPLIT[1] + 1)).0-pre"
	else
		# Patch releases do not get branches
		MAKE_BRANCH=
		BRANCH_VERSION=

		# The next release version will always be another patch version
		PRE_VERSION="${PRE_VERSION_SPLIT[0]}.${PRE_VERSION_SPLIT[1]}.$((PRE_VERSION_SPLIT[2] + 1))-pre"
	fi
else
	MAKE_BRANCH=
	BRANCH_VERSION=
	PRE_VERSION="$VERSION_CARGO_NO_TAGS-pre"
fi

# Some projects prepend "v" to the tag, so this is kept separate in case this
# is the case
TAG_VERSION=$VERSION

# At this point:
# $VERSION is the version of Drawpile that is being released
# $TAG_VERSION is the name that will be used for the Git tag for the release
# $PRE_VERSION is the next pre-release version of Drawpile that will be set on
# the original branch after tagging
# $MAKE_BRANCH is the name of the new minor release branch that should be
# created (if this is not a patch release)
# $BRANCH_VERSION is the pre-release version of Drawpile that will be set on the
# minor release branch

# Something is messed up and this release has already happened
if [ "$(git tag | grep -c "^$TAG_VERSION$")" -gt 0 ]; then
	echo -e "\nTag $TAG_VERSION already exists! Please check the branch.\n"
	exit 1
fi

NOW=$(TZ=UTC date +"%Y-%m-%d")
set_package_version "$VERSION"
sed -i '' -e "s/^Unreleased\\( Version \\).*$/$NOW\\1$VERSION/" ChangeLog
# Using $BUILD_DIR to be explicit that any changes to this script *must* be
# committed to the repository
"$BUILD_DIR/pkg/update-appdata-releases.py"

git commit -m "Updating metadata for $VERSION" -m "[ci skip]" Cargo.toml ChangeLog src/desktop/appdata.xml
git tag -s -m "Release $VERSION" "$TAG_VERSION"

set_package_version "$PRE_VERSION"
echo -e "Unreleased Version $PRE_VERSION\n" | cat - ChangeLog > ChangeLog.new
mv ChangeLog.new ChangeLog

git commit -m "Updating source version to $PRE_VERSION" -m "[ci skip]" Cargo.toml ChangeLog

if [ "$MAKE_BRANCH" != "" ]; then
	git checkout -b "$MAKE_BRANCH" "$TAG_VERSION"
	set_package_version "$BRANCH_VERSION"
	sed -i '' -e "s/^\\(Unreleased Version \\).*$/\\1$BRANCH_VERSION/" ChangeLog

	git commit -m "Updating source version to $BRANCH_VERSION" -m "[ci skip]" Cargo.toml
	PUSH_BRANCHES="$PUSH_BRANCHES $MAKE_BRANCH"
	PUSH_BRANCHES_MSG=" and branches ${PUSH_BRANCHES}"
fi

echo -e "\nDone!\n"

echo "Once updates have been pushed, the following must currently still be done"
echo "manually:"
echo
echo "- Build new server docker image: (in pkg/docker)"
echo "  - docker build -t drawpile/drawpile-srv:x.y.z -t drawpile/drawpile-srv:x.y --build-arg=version=x.y.z ."
echo "  - docker push drawpile/drawpile-srv:x.y.z"
echo "  - docker push drawpile/drawpile-srv:x.y"
echo "- Update Flathub repository (github.com/flathub/net.drawpile.drawpile)"
echo "- Write a news post for the new release"
echo "- Run manage.py templatevar VERSION x.y.z"
echo "- Update templates/pages/help/compatibility.html if needed"
echo
echo "Please confirm packaging success, then press 'y', ENTER to push"
echo "tags $TAG_VERSION${PUSH_BRANCHES_MSG:-}, or any other key to bail."
read -r -p "> "

if [ "$REPLY" != "y" ]; then
	echo "Aborted."
	exit 0
fi

for BRANCH in $PUSH_BRANCHES; do
	git push origin "$BRANCH"
done

git push origin --tags

cd "$ROOT_DIR"
rm -rf "$BUILD_DIR"

echo -e "\nAll done! Yay!"
