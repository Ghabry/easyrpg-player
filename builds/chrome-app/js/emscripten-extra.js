/* Chrome App code */

var index_dict = {};

Module.EASYRPG_STARTUP = function(url_prefix, game_name, userdata, onload, onerror) {
    // prefix and game_name not relevant for the app

    if (standaloneMode) {
        // Forward to default handler with path /game
        var stack = Runtime.stackSave();
        default_startup_handler(
            allocate(intArrayFromString("/"), 'i8', ALLOC_STACK),
            allocate(intArrayFromString("game"), 'i8', ALLOC_STACK),
            userdata,
            onload,
            onerror);
        Runtime.stackRestore(stack);
        return;
    }
    // Generate an index on the fly

    // Files in the root folder must be prefixed with "./" and
    // files in subfolders without extension
    // Key must be lower case
    for (var key in fsEntries) {
        if (fsEntries.hasOwnProperty(key)) {
            var k = removeFolderName(key);
            if (k.indexOf("/") != -1) {
                // subfolder, remove extension
                k = k.substring(0, k.lastIndexOf("."));
            } else {
                // prefix ./
                k = "./" + k;
            }
            index_dict[k.toLowerCase()] = removeFolderName(key);
        }
    }

    var stack = Runtime.stackSave();
    Runtime.dynCall('viii', onload, [0, userdata, allocate(intArrayFromString(""), 'i8', ALLOC_STACK)]);
    Runtime.stackRestore(stack);
}

// Based on emscripten_async_wget2
Module.EASYRPG_WGET = function(url_prefix, game_name, file, userdata, onload, onerror) {
    // prefix and game_name not relevant for the app

    if (standaloneMode) {
        // Forward to default handler with path /game
        var stack = Runtime.stackSave();
        default_wget_handler(
            allocate(intArrayFromString("/"), 'i8', ALLOC_STACK),
            allocate(intArrayFromString("game"), 'i8', ALLOC_STACK),
            file,
            userdata,
            onload,
            onerror);
        Runtime.stackRestore(stack);
        return;
    }

    var _file = Pointer_stringify(file);

    // The dict layout matches the index.json layout,
    // simply forward to the default resolver
    var target_file = Module.EASYRPG_NAME_RESOLVER(_file, index_dict);

    if (target_file === undefined) {
        Runtime.dynCall('viii', onerror, [0, userdata, 404]);
        return;
    }

    var url = currentFolderName + "/" + target_file;
    var reader = new FileReader();

    _file = PATH.resolve(FS.cwd(), _file);
    var index = _file.lastIndexOf('/');
    var destinationDirectory = PATH.dirname(_file);

    reader.onload = function(e) {
        // if a file exists there, we overwrite it
        try {
          FS.unlink(_file);
        } catch (e) {}
        // if the destination directory does not yet exist, create it
        FS.mkdirTree(destinationDirectory);

        FS.createDataFile( _file.substr(0, index), _file.substr(index + 1), new Uint8Array(e.target.result), true, true, false);

        var stack = Runtime.stackSave();
        Runtime.dynCall('viii', onload, [0, userdata, allocate(intArrayFromString(_file), 'i8', ALLOC_STACK)]);
        Runtime.stackRestore(stack);

    };

    reader.onerror = function(e) {
        Runtime.dynCall('viii', onerror, [0, userdata, 400]);
    };

    // Start async request
    fsEntries[url].file(function(file) {
        reader.readAsArrayBuffer(file);
    });
};

var EASYRPG_CHROME_APP_FS = {
    mount: function(mount) {
        // reuse all of the core MEMFS functionality
        return MEMFS.mount.apply(null, arguments);
    },
    read_as_arraybuffer: function(fileEntry, callback) {
        fileEntry.file(function(file) {
            var reader = new FileReader();

            reader.onerror = errorHandler;
            reader.onload = function(e) {
                callback(fileEntry, e.target.result);
            };

            reader.readAsArrayBuffer(file);
        });
    },
    write_from_arraybuffer: function(fileEntry, buffer) {
    },
    syncfs: function(mount, populate, callback) {
        if (populate) {
            // Read game main folder and parse all SaveXX.lsd
            // Populate the virtual filesystem with them
            for (var key in fsEntries) {
                if (fsEntries.hasOwnProperty(key)) {
                    var k = removeFolderName(key);
                    if (k.indexOf("/") == -1) {
                        // root directory, check for savegame
                        if (k.toLowerCase().endsWith(".lsd")) {
                            EASYRPG_CHROME_APP_FS.read_as_arraybuffer(fsEntries[key], function(entry, res) {
                                var stream = FS.open(mount.mountpoint + "/" + entry.name, "w");
                                FS.write(stream, new Uint8Array(res), 0, res.byteLength, 0);
                                FS.close(stream);
                            });
                        }
                    }
                }
            }
        } else {
            var obj = {}
            FS.readdir(mount.mountpoint).forEach(function(x) {
                var num = parseInt(x.substr(4,2));
                if (!isNaN(num) && num >= 1 && num <= 15) {
                    gameBrowserEntry.getFile("Save" + (num <= 9 ? "0" : "") + num + ".lsd", {create:true}, function(file) {
                        file.createWriter(function(writer) {
                            this.onwriteend = function() {
                                this.onwriteend = null;
                                this.truncate(this.position);
                            };
                            writer.write(new Blob([FS.readFile(mount.mountpoint + "/" + x)], { type: 'application/octet-stream' }));
                        });
                    });
                }
            });
        }

        callback(null);
    }
};

Module.EASYRPG_FS = EASYRPG_CHROME_APP_FS;
