# NAME

Drawdance Server Proxy - WebSocket proxy for the web version of Drawdance

# SYNOPSIS

To proxy to a Drawpile server running on localhost on port 27750:

```
go run main.go -dir ../buildem/appdrawdance
```

Then open <http://127.0.0.1:27751/> or start the [drawdance\_web](../appdrawdance/drawdance_web) development server.

To get a list of all available parameters, run `go run main.go -help`.

# DESCRIPTION

This is a small web server written in Go that serves two purposes: it serves Drawdance's static files (HTML, JavaScript, WASM...) and it proxies WebSocket connections to a Drawpile server, translating between WebSocket and plain TCP sockets.

In addition to running it, you can build an executable of the application using `go build`. If you want to build a static executable that doesn't link to any native network libraries, use `go build --tags netgo`. Note that the resulting executable may not be able to resolve hostnames, so i.e. `localhost` may not work while `127.0.0.1` may.

# LICENSE

The Drawdance Server Proxy is licensed under the MIT license. See the [LICENSE-FOR-ORIGINAL-CODE file](../LICENSE-FOR-ORIGINAL-CODE) for the full license text.
