# Drawpile - A Collaborative Drawing Program

[![CI status badge](../../actions/workflows/main.yml/badge.svg)](../../actions/workflows/main.yml) [![translation status](https://hosted.weblate.org/widgets/drawpile/-/svg-badge.svg)](https://hosted.weblate.org/engage/drawpile/)

Drawpile is a drawing program that lets you draw, paint and animate together with others on the same canvas. It runs on Windows, Linux, macOS and Android.

## Installing

Take a look at [the downloads page on drawpile.net](https://drawpile.net/download/) or [the GitHub releases](/drawpile/Drawpile/releases).

Instructions on how to compile Drawpile from source are found [on this documentation page](https://docs.drawpile.net/help/development/buildingfromsource).

If you're using Arch Linux, you can get Drawpile [from the AUR](https://aur.archlinux.org/packages/drawpile).

## Getting Help, Giving Suggestions, Reporting Bugs

If you're having trouble with something, want to suggest a feature or report a bug, take a look at [the help page on drawpile.net](https://drawpile.net/help/).

You can directly [report issues here on GitHub](/drawpile/Drawpile/issues). If you got Discord, you can [join the Drawpile server](https://drawpile.net/discord/) on there. You can also [use the chatroom on libera.chat](https://drawpile.net/irc/), it can be done directly through the browser and doesn't need any account.

## Drawpile on nix

> Linux users hate him for descovering how to reproduce an ENTIRE ENVIROMENT with just ONE COMMAND <br>
> \- Qubic 2023

Having a reproducible work enviroment is nice. 
Especialy if you can do it only with one command.

### Dev enviroment

To get a dev shell with nix you just do `nix shell .#(cmake profile without linux prefix)`
(I did not include linux prefix becuse I'm quite sure it will probably work with nix + macOs)

### Dev shell functions

In dev shells you have 2 very simple functions

`firstBuild` for setuping cmake and building drawpile for first time <br>
`incrementalBuild` for building drawpile successive times

### Package names

We name packages same way we name shell-s

<!---
Sorry for spelling mistakes I wrote this at 3AM tired
TODO rewrite this entierly
-->

## Contributing

Pull requests are welcome, be it for code or anything else! If you want to contribute documentation, you can do so [over in this repository](/drawpile/drawpile.github.io).

If you want to translate Drawpile to your language, take a look at [Drawpile on Weblate](https://hosted.weblate.org/engage/drawpile/). You can translate it directly in the browser.

[![translation status](https://hosted.weblate.org/widgets/drawpile/-/287x66-grey.png)](https://hosted.weblate.org/engage/drawpile/)
