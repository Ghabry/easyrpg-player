// Entry selected through "Choose game directory"
var gameBrowserEntry = null;
// All File entries of the current game
var fsEntries = {};
// Contains the name of the folder of current game
var currentFolderName = "";

// Choose game button
var chooseDirButton = document.querySelector('#choose_dir');
// Play button
var playButton = document.querySelector('#play');
// GameBrowser <div>
var gameBrowser = document.querySelector('#game_browser');
// Player <div>
var player = document.querySelector('#player');

// Hide Player div on startup
player.style.visibility = "hidden";

// standard error handler
function errorHandler(e) {
    console.error(e);
}

function loadDirEntry(gameBrowserEntry, depth) {
    if (depth > 1) {
        // Prevent deep recursion
        return;
    }

    if (gameBrowserEntry.isDirectory) {
        var dirReader = gameBrowserEntry.createReader();

        // Call the reader.readEntries() until no more results are returned.
        // Callback Api is really ugly...
        var readEntries = function() {
            dirReader.readEntries (function(results) {
                if (results.length) {
                    results.forEach(function(item) {
                        if (item.isDirectory) {
                            loadDirEntry(item, depth + 1);
                        } else {
                            fsEntries[item.fullPath] = item;
                        }
                    });
                    readEntries();
                }
            }, errorHandler);
        };

        // Start reading dirs
        readEntries();
    }
}

var index_json;

chooseDirButton.addEventListener('click', function(e) {
    chrome.fileSystem.chooseEntry({type: 'openDirectory'}, function(theEntry) {
        if (!theEntry) {
            // No Directory selected
            return;
        }

        gameBrowserEntry = theEntry;

        // TODO: use local storage to retain access to this file
        //chrome.storage.local.set({'chosenFile': chrome.fileSystem.retainEntry(theEntry)});

        loadDirEntry(theEntry, 0);
    });
});

// Removes the first folder in the path
function removeFolderName(s) {
    return s.substring(s.indexOf("/", 1) + 1)
}

function replaceAll(str, find, replace) {
    return str.replace(new RegExp(find, 'g'), replace);
}

playButton.addEventListener('click', function(e) {
    // Game subfolder name
    currentFolderName = gameBrowserEntry.fullPath;

    // Generate an index.json on the fly
    var dict = {};
    for (var key in fsEntries) {
        if (fsEntries.hasOwnProperty(key)) {
            var k = removeFolderName(key);
            if (k.lastIndexOf(".") != 0 && k.indexOf("/") != 1) {
                // subfolder, remove extension
                k = k.substring(0, k.lastIndexOf("."));
            }
            dict[k.toLowerCase()] = removeFolderName(key);
        }
    }
    index_json=new TextEncoder("utf-8").encode(replaceAll(JSON.stringify(dict), "/", "\\/"));

    // Launch the Player
    var script = document.createElement('script');
    script.src = "player/index.js";
    document.body.appendChild(script);

    // Show the Player
    player.style.visibility = "visible";
    gameBrowser.style.visibility = "hidden";
});
