# Web packaging: from source files to the board

The web interface is four files: `index.html`, `style.css`, `app.js` and
`api.js`. They reach the board in five steps.

1. **Minify.** `web/scripts/build.py` strips comments and extra whitespace.
   It never renames or restructures code, so behaviour cannot change.
2. **Gzip.** Each file is compressed into `web/dist`, for example
   `app.js.gz`. The build fails if the total passes 60 KB, the budget we
   set because the board's flash and memory are shared with encryption and
   the event log. Today the total is about 11.8 KB.
3. **Pack into a LittleFS image.** LittleFS is a small file system designed
   for flash memory. The four `.gz` files are packed into one image file the
   exact size of the board's `www` partition. Either `mklittlefs` does this,
   or the ESP-IDF build does it automatically. A verifier reopens the image
   and checks every file matches byte for byte.
4. **Flash to the www partition.** The board's 8 MB flash is divided into
   partitions, like a disk: one holds the program, one holds settings, and
   `www` holds the web files. Flashing the image writes only that partition,
   so the web interface can be updated without touching the program.
5. **Serve.** At boot the firmware mounts `www` as a file system. When a
   browser asks for `/app.js`, the HTTPS server sends `app.js.gz` with a
   header saying it is gzipped, and the browser unpacks it.

The files stay compressed on the board and over the network. Only the
browser ever unpacks them.
