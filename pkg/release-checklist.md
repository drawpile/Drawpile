# Drawpile Release Checklist

This checklist uses pseudo-placeholders. $VERSION is version to be released, $NEXTVERSION is the (most likely) one after that. Don't type them in literally.

Yes, this can be automated to a large degree. It just hasn't happened yet because the release flow is still in flux.

* Translations:
    * Press the "Commit" button on Weblate if there's pending changes: <https://hosted.weblate.org/projects/drawpile/#repository>. Wait for Weblate to create or update the pull request.
    * Update translators in AUTHORS if there's new ones.
    * Merge Weblate pull request, if there is one, into main. Pull main.
* Update ChangeLog:
    * Change heading from "Unreleased Version $VERSION-pre" to "YYYY-mm-dd Version $VERSION".
    * Add a new heading at the top saying "Unreleased Version $NEXTVERSION-pre".
    * Create a signed commit "Update changelog for $VERSION"
* Update appdata XMLs:
    * Run `pkg/update-appdata-releases.py` to generate drawpile.appdata.xml
    * Run `pkg/update-appdata-releases.py --legacy` to generate net.drawpile.drawpile.appdata.xml
    * Create a signed commit "Update appdata XMLs for $VERSION"
* Update Cargo version:
    * Run `pkg/update-cargo-version.bash $VERSION`.
    * Build once locally so that the Cargo.lock file gets updated.
    * Create a signed commit "Update Cargo files for $VERSION"
* Preliminary build:
    * Consider deleting the caches for Qt and other dependencies for the Windows target, because the release will use link-time optimization and the MSVC version must match for that to work: <https://github.com/drawpile/Drawpile/actions/caches>
    * Push and let it build. This takes a while because of the above.
* Build:
    * Create a signed version tag: `git tag -sm "Release $VERSION" "$VERSION"`
    * Push the tag: `git push origin "$VERSION"`
    * Change directories pkg/docker, build and push drawpile-srv for the version you're releasing and with the last segment chopped off (e.g. 2.2.0 and 2.2) for x86_64 and ARM64. `docker buildx build -t drawpile/drawpile-srv:$VERSION -t drawpile/drawpile-srv:"$(echo "$VERSION" | sed 's/\.[0-9+]$//')" --platform linux/amd64,linux/arm64 --build-arg version=$VERSION --push .`
    * Let GitHub and Docker build it.
* Update Flatpak:
    * Clone the repo <https://github.com/flathub/net.drawpile.drawpile>
    * Check out a new branch called $VERSION
    * In `net.drawpile.drawpile.yaml`, at the bottom under `sources`, update `tag` to $VERSION and `commit` to the full commit hash of that tag's commit.
    * Create a signed commit with the message "Release $VERSION".
    * Push.
    * Create a pull request into `master`, flathubbot will kick off a test build. If it doesn't, comment with "bot, build" to kick it off manually.
    * Wait for the test build to succeed.
    * Merge the pull request.
* Add artifacts to appdata XMLs:
    * Create an `artifacts` directory.
    * Download Linux AppImage, Android APKs (64 and 32 bit), macOS Disk Image, Windows Installers (64 and 32 bit) and source code in tar.gz format into this directory.
    * Run `pkg/update-appdata-releases.py --artifacts` to add artifact links and checksums to drawpile.appdata.xml
    * Run `pkg/update-appdata-releases.py --artifacts --legacy` to add them to net.drawpile.drawpile.appdata.xml
    * Delete `artifacts` directory
    * Create a signed commit "Add $VERSION artifacts to appdata XMLs"
* Update Cargo version for the next release:
    * Run `pkg/update-cargo-version.bash $NEXTVERSION-pre`.
    * Build locally so that the Cargo.lock file gets updated.
    * Create a signed commit "Update Cargo files for $NEXTVERSION-pre"
    * Push
* Website:
    * `./manage.py templatevar VERSION $VERSION` (or BETAVERSION)
    * Write a news post and publish it.
    * Upload `net.drawpile.drawpile.appdata.xml` to the website, chown it to webfiles:webfiles and move it to `/home/webfiles/www/metadata`
* Update news.json:
    * News post about the update.
    * Entry in the updates section.
    * Upload it to the website, chown it to webfiles:webfiles and move it to `/home/webfiles/www/metadata`
* Announce on social channels.
