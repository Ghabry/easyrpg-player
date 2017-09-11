/* Chrome App code */

// Taken from emscripten Browser library
var nextWgetRequestHandle = 0;
var getNextWgetRequestHandle = function() {
    var handle = nextWgetRequestHandle;
    nextWgetRequestHandle++;
    return handle;
};
var wgetRequests = {};

// Based on emscripten_async_wget2
Module.EASYRPG_WGET = function(url, file, userdata, onload, onerror) {
    var _url = Pointer_stringify(url);

    // TODO make this nicer
    _url = _url.substring(20); // Remove /player/games/default
    if (_url[0] == "." && _url[1] == "/") {
        _url = _url.substring(2);
    }

    if (standaloneMode) {
        _url = "game/" + _url;

        // Forward to emscripten ajax
        var stack = Runtime.stackSave();
        var handle = Module._emscripten_async_wget2(
            allocate(intArrayFromString(_url), 'i8', ALLOC_STACK),
            file,
            allocate(intArrayFromString("GET"), 'i8', ALLOC_STACK),
            0,
            userdata,
            onload,
            onerror,
            0
        );
        Runtime.stackRestore(stack);

        return handle;
    }

    _url = currentFolderName + "/" + _url;

    var _file = Pointer_stringify(file);
    _file = PATH.resolve(FS.cwd(), _file);

    var _request = "GET";
    var index = _file.lastIndexOf('/');

    var handle = getNextWgetRequestHandle();

    var destinationDirectory = PATH.dirname(_file);

    // Return our custom index.json
    if (_url == currentFolderName + "/index.json") {
        // FIXME: Lots of code-dupe with onload
        try {
            FS.unlink(_file);
        } catch (e) {}
        // if the destination directory does not yet exist, create it
        FS.mkdirTree(destinationDirectory);

        FS.createDataFile( _file.substr(0, index), _file.substr(index + 1), index_json, true, true, false);

        var stack = Runtime.stackSave();
        Runtime.dynCall('viii', onload, [handle, userdata, allocate(intArrayFromString(_file), 'i8', ALLOC_STACK)]);
        Runtime.stackRestore(stack);
        delete wgetRequests[handle];
        return;
    }

    if (!fsEntries.hasOwnProperty(_url)) {
        if (onerror) Runtime.dynCall('viii', onerror, [handle, userdata, 400]);
        delete wgetRequests[handle];
        return;
    }

    var reader = new FileReader();

    reader.onload = function em_onload(e) {
        // if a file exists there, we overwrite it
        try {
          FS.unlink(_file);
        } catch (e) {}
        // if the destination directory does not yet exist, create it
        FS.mkdirTree(destinationDirectory);

        FS.createDataFile( _file.substr(0, index), _file.substr(index + 1), new Uint8Array(e.target.result), true, true, false);
        if (onload) {
            var stack = Runtime.stackSave();
            Runtime.dynCall('viii', onload, [handle, userdata, allocate(intArrayFromString(_file), 'i8', ALLOC_STACK)]);
            Runtime.stackRestore(stack);
        }

        delete wgetRequests[handle];
    };

    reader.onerror = function em_onerror(e) {
        if (onerror) Runtime.dynCall('viii', onerror, [handle, userdata, 400]);
        delete wgetRequests[handle];
    };

    wgetRequests[handle] = reader;

    // Start async request
    fsEntries[_url].file(function(file) {
        reader.readAsArrayBuffer(file);
    });

    return handle;
};
