# Drawpile Release Checklist

This checklist uses pseudo-placeholders. $VERSION is version to be released, $NEXTVERSION is the (most likely) one after that. Don't type them in literally.

Yes, this can be automated to a large degree, but it currently happens infrequently enough that manual changes are required every time.

* Translations:
    * Press the "Commit" button on Weblate if there's pending changes: <https://hosted.weblate.org/projects/drawpile/#repository>. Wait for Weblate to create or update the pull request.
    * Update translators in AUTHORS if there's new ones.
    * Merge Weblate pull request, if there is one, into main. Pull main.
* Update ChangeLog:
    * Change heading from "Unreleased Version $VERSION-pre" to "YYYY-mm-dd Version $VERSION".
    * Add a new heading at the top saying "Unreleased Version $NEXTVERSION-pre".
    * Create a signed commit "Update changelog for $VERSION"
* Update appdata XMLs:
    * If you're building a stable release and there's beta releases in drawpile.appdata.xml and net.drawpile.drawpile.appdata.xml, remove the beta entries. Appdata has broken version ordering, it thinks that the beta versions come *after* the final release, so we can't keep them in there.
    * Run `pkg/update-appdata-releases.py` to generate drawpile.appdata.xml
    * Run `pkg/update-appdata-releases.py --legacy` to generate net.drawpile.drawpile.appdata.xml
    * Create a signed commit "Update appdata XMLs for $VERSION"
* Update F-Droid metadata:
    * Update the version name and code in `metadata/fdroidversion.txt`
    * Create four identical changelog files in `metadata/en-US/changelogs` with a short change description (500 characters max)
    * Create a signed commit "Update F-Droid metadata for $VERSION"
* Update Cargo version:
    * Run `pkg/update-cargo-version.bash $VERSION`.
    * Build once locally so that the Cargo.lock file gets updated.
    * Create a signed commit "Update Cargo files for $VERSION"
* Preliminary build:
    * Consider deleting the caches for Qt and other dependencies for the Windows target, because the release will use link-time optimization and the MSVC version must match for that to work: <https://github.com/drawpile/Drawpile/actions/caches>
    * Push and let it build. This takes a while because of the above.
* Build:
    * Create a signed version tag: `git tag -sm "Release $VERSION" "$VERSION"`
    * Install the SignPath application with access to the Drawpile repo: <https://github.com/apps/signpath>
    * Push the tag: `git push origin "$VERSION"`
    * Let it build.
    * Once built, uninstall Signpath again.
* Update Docker:
    * Change directories pkg/docker, build and push drawpile-srv for the version you're releasing. If you're releasing for a stable version, also add a tag with the last segment chopped off (e.g. 2.2.0 and 2.2) for x86_64 and ARM64. `docker buildx build -t drawpile/drawpile-srv:$VERSION -t drawpile/drawpile-srv:"$(echo "$VERSION" | sed 's/\.[0-9+]$//')" --platform linux/amd64,linux/arm64 --build-arg version=$VERSION --push .`
    * Let it build. It'll automatically push.
    * Prune the buildx guff: `docker buildx prune` - do it now, if you try to do it later the container will just say it isn't running and then you have to start another build and cancel it just so you can prune.
* Update Flatpak:
    * Clone the repo <https://github.com/flathub/net.drawpile.drawpile>
    * Check out a new branch called $VERSION, either from the `master` branch if building stable or from the `beta` branch when building a beta.
    * In `net.drawpile.drawpile.yaml`, at the bottom under `sources`, update `tag` to $VERSION and `commit` to the full commit hash of that tag's commit.
    * Create a signed commit with the message "Release $VERSION".
    * Push.
    * Create a pull request into the appropriate branch, flathubbot will kick off a test build. If it doesn't, comment with "bot, build" to kick it off manually.
    * Wait for the test build to succeed.
    * Merge the pull request.
* Add artifacts to appdata XMLs:
    * Create an `artifacts` directory.
    * Download all assets:
        * `gh release download $VERSION -D artifacts`
        * `gh release download $VERSION -D artifacts --archive=tar.gz`
        * `gh release download $VERSION -D artifacts --archive=zip`
    * Download Linux AppImage, Android APKs (64 and 32 bit), macOS Disk Images (ARM and Intel), Windows Installers (64 and 32 bit) and source code in tar.gz format into this directory.
    * Run `pkg/update-appdata-releases.py --artifacts` to add artifact links and checksums to drawpile.appdata.xml. If this fails because the Android build versions or something have changed, update them in the script.
    * Run `pkg/update-appdata-releases.py --artifacts --legacy` to add them to net.drawpile.drawpile.appdata.xml.
    * Delete `artifacts` directory
    * Create a signed commit "Add $VERSION artifacts to appdata XMLs"
* Create checksum files:
    * Create a directory `pkg/checksums/$VERSION`
    * Change into the `artifacts` directory and run checksums:
        * `sha256sum *.* >../pkg/checksums/$VERSION/sha256sums.txt`
        * `sha384sum *.* >../pkg/checksums/$VERSION/sha384sums.txt`
        * `b2sum *.* >../pkg/checksums/$VERSION/b2sums.txt`
    * Create a signed commit "Add $VERSION artifact checksums"
    * Upload that directory to the website, chown it to webfiles:webfiles and move it to `/home/webfiles/www/checksums`.
* Update Cargo version for the next release:
    * Run `pkg/update-cargo-version.bash $NEXTVERSION-pre`.
    * Build locally so that the Cargo.lock file gets updated.
    * Create a signed commit "Update Cargo files for $NEXTVERSION-pre"
    * Push
* Website:
    * For a stable release, update the version with `./manage.py templatevar VERSION $VERSION`. For a beta, use BETAVERSION instead of VERSION.
    * Write a news post and publish it.
    * Upload `net.drawpile.drawpile.appdata.xml` to the website, chown it to webfiles:webfiles and move it to `/home/webfiles/www/metadata`
* Update news.json:
    * News post about the update.
    * Entry in the updates section.
    * Upload it to the website, chown it to webfiles:webfiles and move it to `/home/webfiles/www/metadata`
* Announce on social channels.
