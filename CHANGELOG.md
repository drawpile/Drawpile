# CHANGELOG

Each changelog entry contains a date in Year-Month-Day format and the version number in its title. Below follows list of changes that may not be wholly complete, check the git history if you want all the things.

## 2022-02-20 - v0.2.0

* Pull version number from configuration instead of hard-coding it.

* Set the max-length of the web username input to 22, since that's the maximum drawpile-srv allows anyway.

* Allow serving the web frontend from paths other than /.

* Change the default WebSocket proxy path to be under `BASE_URL/ws` instead of sharing the static file paths, which simplifies the proxy server a little.

## 2022-02-15 - v0.1.0

* Initial release.
