// Minimal offline shell: serve cached files immediately and refresh the cache
// in the background (stale-while-revalidate). Bump CACHE to force an update.
const CACHE = 'team-tree-v1';
const ASSETS = [
  './',
  './index.html',
  './style.css',
  './app.js',
  './effects.js',
  './manifest.webmanifest',
  './icon.svg',
  'https://unpkg.com/mqtt@5.10.1/dist/mqtt.min.js',
];

self.addEventListener('install', (event) => {
  event.waitUntil(caches.open(CACHE).then((c) => c.addAll(ASSETS)).then(() => self.skipWaiting()));
});

self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys()
      .then((keys) => Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k))))
      .then(() => self.clients.claim()),
  );
});

self.addEventListener('fetch', (event) => {
  if (event.request.method !== 'GET') return;
  event.respondWith(
    caches.open(CACHE).then(async (cache) => {
      const cached = await cache.match(event.request);
      const network = fetch(event.request)
        .then((res) => {
          if (res.ok) cache.put(event.request, res.clone());
          return res;
        })
        .catch(() => cached);
      return cached || network;
    }),
  );
});
