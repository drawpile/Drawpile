# Drawpile - A Collaborative Drawing Program

[![CI status badge](../../actions/workflows/main.yml/badge.svg)](../../actions/workflows/main.yml) [![translation status](https://hosted.weblate.org/widgets/drawpile/-/svg-badge.svg)](https://hosted.weblate.org/engage/drawpile/)

Drawpile is a drawing program that lets you draw, paint and animate together with others on the same canvas. It runs on Windows, Linux, macOS and Android.

## Installing

Take a look at [the downloads page on drawpile.net](https://drawpile.net/download/) or [the GitHub releases](https://github.com/drawpile/Drawpile/releases).

Instructions on how to compile Drawpile from source are found [on this documentation page](https://docs.drawpile.net/help/development/buildingfromsource).

If you're using Arch Linux, you can get Drawpile [from the AUR](https://aur.archlinux.org/packages/drawpile).

## Getting Help, Giving Suggestions, Reporting Bugs

If you're having trouble with something, want to suggest a feature or report a bug, take a look at [the help page on drawpile.net](https://drawpile.net/help/).

You can directly [report issues here on GitHub](https://github.com/drawpile/Drawpile/issues). If you got Discord, you can [join the Drawpile server](https://drawpile.net/discord/) on there. You can also [use the chatroom on libera.chat](https://drawpile.net/irc/), it can be done directly through the browser and doesn't need any account.

## Contributing

Pull requests are welcome, be it for code or anything else! If you want to contribute documentation, you can do so [over in this repository](https://github.com/drawpile/drawpile.github.io).

If you want to translate Drawpile to your language, take a look at [Drawpile on Weblate](https://hosted.weblate.org/engage/drawpile/). You can translate it directly in the browser.

[![translation status](https://hosted.weblate.org/widgets/drawpile/-/287x66-grey.png)](https://hosted.weblate.org/engage/drawpile/)

## Client Dependencies

The Drawpile client uses the following shared libraries:

* Qt (all platforms)
* OpenSSL (all platforms)
* KDE Framework Archive (Windows, Linux AppImage, Android)
* libzip (macOS, Linux Flatpak)

On Windows, these libraries are signed along with the executable using free code signing provided by [SignPath.io](https://about.signpath.io/) and a  certificate by [SignPath Foundation](https://signpath.org/). See [the code signing policy on drawpile.net](https://drawpile.net/codesigningpolicy/) for details.

The dependencies are pinned to known good versions and the source code for is verified against the hashes and signatures provided in their releases from upstream. SHA384 hash checks are also done for each build to ensure integrity of the source code retrieved from upstream.

We make some patches to these dependencies when building the application, which you can find in [.github/scripts/patches](.github/scripts/patches). Each patch file contains a description as to what it does.

You can find build processes, versions, the upstream source URLs and hashes [for Qt and OpenSSL here](.github/scripts/build-qt.cmake) and [for KDE Framework Archive and libzip here](.github/scripts/build-other.cmake).
