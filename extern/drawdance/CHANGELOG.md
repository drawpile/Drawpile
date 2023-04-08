# CHANGELOG

Each changelog entry contains a date in Year-Month-Day format and the version number in its title. Below follows list of changes that may not be wholly complete, check the git history if you want all the things.

## 2022-07-20 - v0.3.0

* web: make the chat scroll down properly (thanks cow.)

* web: use a textarea for chat input to allow line breaks (thanks cow.)

* Compile with a current Emscripten version, hoping that it'll cut down on the spurious internal threading-related errors in the browser.

* Turn on strict aliasing in release builds.

* Fix blank tile check, it was inverted.

* Add SDL and Qt threading implementations, for future Windows and Android compatibility.

* Various preparations for using Drawdance's paint engine in Drawpile, such as:

    * C++ and Qt compatibility, making SDL optional for libcommon, libmsg and libengine.

    * Splitting the layer model into layer content and layer properties to allow for diffing them separately.

    * Adding local fork capabilities to the canvas history.

    * Making the engine understand annotations, although it can't render them currently.

## 2022-02-20 - v0.2.0

* Pull version number from configuration instead of hard-coding it.

* Set the max-length of the web username input to 22, since that's the maximum drawpile-srv allows anyway.

* Allow serving the web frontend from paths other than /.

* Change the default WebSocket proxy path to be under `BASE_URL/ws` instead of sharing the static file paths, which simplifies the proxy server a little.

## 2022-02-15 - v0.1.0

* Initial release.
