function parseargs() {
    var tmp = [];
    var ret = [];
    var items = location.search.substr(1).split("&");

    // Store saves in subdirectory Save
    ret.push("--save-path");
    ret.push("Save");

    for (var index = 0; index < items.length; index++) {
        tmp = items[index].split("=");

        if (tmp[0] == "project-path" || tmp[0] == "save-path") {
            // Filter arguments that are set by us
            continue;
        }

        if (tmp[0] == "game") {
            // Move to different directory to prevent Save file collisions in IDBFS
            if (tmp.length > 1) {
                tmp[1] = tmp[1].toLowerCase();
                FS.mkdir(tmp[1]);
                FS.chdir(tmp[1]);
            }
        }
        ret.push("--" + tmp[0]);
        if (tmp.length > 1) {
            arg = decodeURI(tmp[1]);
            // split except if it's a string
            if (arg.length > 0) {
                if (arg.slice(0) == "\"" && arg.slice(-1) == "\"") {
                    ret.push(arg.slice(1, -1));
                } else {
                    var spl = arg.split(" ");
                    ret = ret.concat(spl);
                }
            }
        }
    }
    return ret;
}

Module.arguments.push("easyrpg-player");
Module.arguments = Module.arguments.concat(parseargs());

var file_index = {};

// Default functions for startup handling and file requests
var default_startup_handler = function(url_prefix, game_name, userdata, onload, onerror) {
    // Download index.json here
    var _url_prefix = Pointer_stringify(url_prefix);
    var _game_name = Pointer_stringify(game_name);

    if (_game_name == "") {
        _game_name = "default";
    }

    var request_url = _url_prefix + _game_name + "/";

    console.log("STARTUP " + request_url);

    var req = new XMLHttpRequest();
    req.open("GET", request_url + "index.json", true);

    req.onload = function(e) {
        if (req.status == 200) {
            file_index = JSON.parse(req.responseText);

            var stack = Runtime.stackSave();
            Runtime.dynCall('viii', onload, [0, userdata, allocate(intArrayFromString(""), 'i8', ALLOC_STACK)]);
            Runtime.stackRestore(stack);
        } else {
            Runtime.dynCall('viii', onerror, [0, userdata, req.status]);
        }
    }

    req.onerror = function(e) {
        Runtime.dynCall('viii', onerror, [0, userdata, req.status]);
    }

    req.send(null);
}

var default_name_resolver = function(filename) {
    var lf = filename.toLowerCase();

    console.log("RESOLVE " + filename + " to " + lf[filename]);

    // Looks in the index.json and returns the value
    if (file_index.hasOwnProperty(lf)) {
        return file_index[lf];
    }

    return undefined;
}

var default_wget_handler = function(url_prefix, game_name, file, userdata, onload, onerror) {
    var _url_prefix = Pointer_stringify(url_prefix);
    var _game_name = Pointer_stringify(game_name);
    var _file = Pointer_stringify(file);

    if (_game_name == "") {
        _game_name = "default";
    }

    var request_url = _url_prefix + _game_name + "/";

    var target_file = Module.EASYRPG_NAME_RESOLVER(_file);

    if (target_file === undefined) {
        Runtime.dynCall('viii', onerror, [0, userdata, 404]);
        return;
    }

    request_url += target_file;

    console.log("WGET " + request_url);

    var replaceAll = function(target, search, replacement) {
        return target.replace(new RegExp(search, 'g'), replacement);
    };

    // Handle special characters
    request_url = replaceAll(request_url, "%", "%25");
    request_url = replaceAll(request_url, "#", "%23");

    var stack = Runtime.stackSave();
    Module._emscripten_async_wget2(
        allocate(intArrayFromString(request_url), 'i8', ALLOC_STACK),
        file,
        allocate(intArrayFromString("GET"), 'i8', ALLOC_STACK),
        0,
        userdata,
        onload,
        onerror,
        0
    );
    Runtime.stackRestore(stack);
}

// Use IDBFS for Save storage when the filesystem was not
// overwritten by a custom emscripten shell file
if (typeof(Module.EASYRPG_FS) === "undefined") {
    Module.EASYRPG_FS = IDBFS;
}

// Use default startup handler when not overwritten
if (typeof(Module.EASYRPG_STARTUP) === "undefined") {
    Module.EASYRPG_STARTUP = default_startup_handler;
}
var ptr = Runtime.addFunction(Module.EASYRPG_STARTUP);
Module._set_startup_handler(ptr);

// Use default file download handler when not overwritten
if (typeof(Module.EASYRPG_WGET) === "undefined") {
    Module.EASYRPG_WGET = default_wget_handler;
}
ptr = Runtime.addFunction(Module.EASYRPG_WGET);
Module._set_request_handler(ptr);

// Use default name resolver when not overwritten
// not called from C code, no pointer indirection needed
if (typeof(Module.EASYRPG_NAME_RESOLVER) === "undefined") {
    Module.EASYRPG_NAME_RESOLVER = default_name_resolver;
}
