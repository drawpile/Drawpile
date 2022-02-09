# NAME

drawdance\_web - React-based Web Interface for Drawdance

# SYNOPSIS

First, run `npm install --include=dev` to install node dependencies.

For development:

- Build the application for Emscripten.

- Start the [drawdance-server-proxy](../../server) and point it at the build directory.

- Edit the [drawdance-config.js](public/drawdance-config.js) and set the `targetUrl` to `ws://localhost:27751`.

- Run `npm start` in this directory to start the development server.

For production:

- Build the application for Emscripten in release mode.

- Run `npm run build` in this directory. The resulting directory will be called `build`.

- Copy `drawdance.{data,js,wasm,worker.js}` from the Emscripten build directory into `build`.

# DESCRIPTION

This is a web frontend for Drawdance. It's built on React and acts as a shell around the Emscripten build of Drawdance.

Like Drawdance, it's currently only built for watching, not drawing yourself. It requires a browser that supports SharedArrayBuffer, since Drawdance runs on multiple threads.

For deploying this in production, you probably need a TLS certificate. When SharedArrayBuffer is involved, browsers only allow WebSocket connections under localhost unsecured, all other domains need to be HTTPS/WSS secured.

To generate the [third-party license attributions](attribution.txt), [oss-attribution-generator](https://github.com/zumwald/oss-attribution-generator) is used. You should run it after modifying the dependencies to keep that document up to date.

# LICENSE

Drawdance is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version. See the [LICENSE file](../../LICENSE) for the full license text.

The drawdance\_web code itself is licensed under the MIT license. See the [LICENSE-FOR-ORIGINAL-CODE file](../LICENSE-FOR-ORIGINAL-CODE) for the full license text.

# SEE ALSO

- [The Drawdance README](../../README.md)

- [The drawdance-server-proxy README](../../server/README.md)
