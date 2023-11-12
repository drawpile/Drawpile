# Drawpile Release Checklist

This checklist uses pseudo-placeholders. $VERSION is version to be released, $NEXTVERSION is the (most likely) one after that. Don't type them in literally.

* Translations:
    * Press the "Commit" button on Weblate if there's pending changes: <https://hosted.weblate.org/projects/drawpile/#repository>. Wait for Weblate to create or update the pull request.
    * Update translators in AUTHORS if there's new ones.
    * Merge Weblate pull request, if there is one, into main. Pull main.
* Update ChangeLog:
    * Change heading from "Unreleased Version $VERSION-pre" to "YYYY-mm-dd Version $VERSION".
    * Add a new heading at the top saying "Unrelease Version $NEXTVERSION-pre".
    * Create a signed commit "Update changelog for $VERSION"
    * Consider deleting the caches for Qt and other dependencies for the Windows target, because the release will use link-time and the MSVC version must match for that to work: <https://github.com/drawpile/Drawpile/actions/caches>
    * Push and let it build. This takes a while because of the above.
* Build:
    * Create a signed version tag: `git tag -sm "Release $VERSION" "$VERSION"`
    * Push the tag: `git push origin "$VERSION"`
    * Let it build.
* Website:
    * `./manage.py templatevar VERSION $VERSION` (or BETAVERSION)
    * Write a news post and publish it.
* Update appdata XMLs:
    * Create an `artifacts` directory.
    * Download Linux AppImage, Android APK, macOS Disk Image, Windows Installer and source code in tar.gz format into this directory.
    * Run `pkg/update-appdata-releases.py --out=drawpile.appdata.xml --legacy`
    * Copy the release into `appdata.xml` and remove "win64" and "macos" entries.
    * Copy the release into `net.drawpile.drawpile.appdata.xml` and remove "x86_64-windows-msvc" and "x86_64-darwin-gnu" entries.
    * Delete `artifacts` directory and `drawpile.appdata.xml`.
    * Commit with [ci skip] and push.
    * Upload `net.drawpile.drawpile.appdata.xml` to the website, chown it to webfiles:webfiles and move it to `/home/webfiles/www/metadata`
* Update news.json:
    * News post about the update.
    * Entry in the updates section.
    * Upload it to the website, chown it to webfiles:webfiles and move it to `/home/webfiles/www/metadata`
* Announce on social channels.
