// Entry selected through "Choose game directory"
var gameBrowserEntry = null;
// All File entries of the current game
var fsEntries = {};
// Contains the name of the folder of current game
var currentFolderName = "";
// true when standalone mode is enabled (game running from "game" folder)
var standaloneMode = false;

// Choose game button
var chooseDirButton = document.querySelector('#choose_dir');
// Play button
var playButton = document.querySelector('#play');
// Frontend <div>
var frontend = document.querySelector('#frontend');
// Player <div>
var player = document.querySelector('#player');
// Website icon
var website = document.querySelector('#website');
// Game list <div>
var gamelist = document.querySelector('#game-grid');

// Hide both divs on startup
frontend.style.visibility = "hidden";
player.style.visibility = "hidden";

// standard error handler
function errorHandler(e) {
    console.error(e);
}

function loadDirEntry(dirEntry, max_depth, callback) {
    if (max_depth < 0) {
        // Prevent deep recursion
        return;
    }

    if (dirEntry.isDirectory) {
        var dirReader = dirEntry.createReader();

        // Call the reader.readEntries() until no more results are returned.
        // Callback Api is really ugly...
        var readEntries = function() {
            dirReader.readEntries (function(results) {
                if (results.length) {
                    for (var i = 0; i < results.length; ++i) {
                        var item = results[i];
                        if (item.isDirectory) {
                            loadDirEntry(item, max_depth - 1, callback);
                        } else {
                            if (!callback(item, dirEntry)) {
                                break;
                            }
                        }
                    }
                    readEntries();
                }
            }, errorHandler);
        };

        // Start reading dirs
        readEntries();
    }
}

function addGame(dirEntry) {
    var elem = document.createElement("div");
    //elem.innerHTML = dirEntry.name;
    gamelist.appendChild(elem);

    dirEntry.getDirectory('Title', {}, function(systemEntry) {
        loadDirEntry(systemEntry, 0, function(item) {
            if (!item.isDirectory) {
                readFileToArrayBuffer(item, function(res) {
                    var arr = new Uint8Array(res);
                    var img = document.createElement("img");
                    img.src = 'data:image/png;base64,' + uint8ArrayToBase64(arr);
                    elem.appendChild(img);
                });
                return false;
            }
            return true;
        });
    });
}

/* Event handlers */
chooseDirButton.addEventListener('click', function(e) {
    chrome.fileSystem.chooseEntry({type: 'openDirectory'}, function(theEntry) {
        if (!theEntry) {
            // No Directory selected
            return;
        }

        gameBrowserEntry = theEntry;

        // use local storage to retain access to this file
        chrome.storage.local.set(
            {'gameBrowserDir': chrome.fileSystem.retainEntry(theEntry)}
        );

        loadDirEntry(theEntry, 3, function(item, dirEntry) {
            //console.log("XGAME " + dirEntry.name);
            if (item.name.toLowerCase() == "rpg_rt.ldb") {
                addGame(dirEntry);
                return false;
            }
            return true;
        });
    });
});

playButton.addEventListener('click', function(e) {
    // Game subfolder name
    currentFolderName = gameBrowserEntry.fullPath;

    startGame();
});

website.addEventListener('click', function(e) {
    // Needs Chrome 42
    chrome.browser.openTab({"url": "https://easyrpg.org"});
    e.preventDefault();
});

/* Test for standalone mode */
var xhr = new XMLHttpRequest();
xhr.open('GET', 'game/index.json');
xhr.onreadystatechange = function () {
    if (xhr.readyState === 4) {
        if (xhr.status === 200) {
            standaloneMode = true;
            startGame();
        } else {
            // No standalone mode, use game browser

            // Restore previous folder from local storage
            chrome.storage.local.get('gameBrowserDir', function (result) {
                chrome.fileSystem.restoreEntry(result.gameBrowserDir, function (theEntry) {
                    gameBrowserEntry = theEntry;
                    loadDirEntry(theEntry, 3, function(item, dirEntry) {
                        //console.log("XGAME " + dirEntry.name);
                        if (item.name.toLowerCase() == "rpg_rt.ldb") {
                            addGame(dirEntry);
                            return false;
                        }
                        return true;
                    });
                    /*loadDirEntry(theEntry, 1, function(item) {
                        fsEntries[item.fullPath] = item;
                    });*/
                });
            });

            // Show game browser
            frontend.style.visibility = "visible";
        }
    }
};
xhr.send(null)
