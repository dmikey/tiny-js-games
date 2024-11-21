
const CACHE_NAME = 'debug-dungeon-tcg-cache-v1';
const urlsToCache = [
    '/',
    '/index.html',
    '/css/style.css',
    '/manifest.json',
    '/binjgb.js',
    '/js/script.js',
    '/js/debugger.js',
    '/roms/game.gb'
];

self.addEventListener('install', function (event) {
    event.waitUntil(
        caches.open(CACHE_NAME)
            .then(function (cache) {
                console.log('Opened cache');
                return cache.addAll(urlsToCache);
            })
    );
});

self.addEventListener('fetch', function (event) {
    event.respondWith(
        caches.match(event.request)
            .then(function (response) {
                if (response) {
                    return response;
                }
                return fetch(event.request);
            })
    );
});