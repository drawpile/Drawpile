// SPDX-License-Identifier: GPL-3.0-or-later
const VERSION = "@CACHEBUSTER@";
const CACHE_NAME = `drawpile-${VERSION}`;
const APP_STATIC_RESOURCES = [
  "/",
  "/index.html",
  "/assets.bundle?cachebuster=@CACHEBUSTER@",
  "/drawpile.css?cachebuster=@CACHEBUSTER@",
  "/drawpile.js?cachebuster=@CACHEBUSTER@",
  "/drawpile.svg?cachebuster=@CACHEBUSTER@",
  "/drawpile.wasm?cachebuster=@CACHEBUSTER@",
  "/drawpile.worker.js?cachebuster=@CACHEBUSTER@",
  "/init.js?cachebuster=@CACHEBUSTER@",
  "/qtloader.js?cachebuster=@CACHEBUSTER@",
];

self.addEventListener("install", (event) => {
  event.waitUntil(
    (async () => {
      const cache = await caches.open(CACHE_NAME);
      cache.addAll(APP_STATIC_RESOURCES);
    })(),
  );
});

self.addEventListener("activate", (event) => {
  event.waitUntil(
    (async () => {
      const names = await caches.keys();
      await Promise.all(
        names.map((name) => {
          if (name !== CACHE_NAME) {
            return caches.delete(name);
          }
        }),
      );
      await clients.claim();
    })(),
  );
});

self.addEventListener("fetch", (event) => {
  if (event.request.mode === "navigate") {
    event.respondWith(caches.match("/"));
  } else {
    event.respondWith(
      (async () => {
        const cache = await caches.open(CACHE_NAME);
        const cachedResponse = await cache.match(event.request.url);
        if (cachedResponse) {
          return cachedResponse;
        } else {
          return await fetch(event.request);
        }
      })(),
    );
  }
});
