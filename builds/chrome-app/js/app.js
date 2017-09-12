// Entry selected through "Choose game directory"
var gameBrowserEntry = null;
// All File entries of the current game
var fsEntries = {};
// All game entries in the game browser
var gameEntries = {};
// Contains the name of the folder of current game
var currentFolderName = "";
// true when standalone mode is enabled (game running from "game" folder)
var standaloneMode = false;

// Choose game button
var chooseDirButton = document.querySelector('#choose_dir');
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

var remaining_dir_entries = 0;

function loadDirEntry(dirEntry, max_depth, callback, finished) {
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
                            loadDirEntry(item, max_depth - 1, callback, finished);
                        } else {
                            if (!callback(item, dirEntry)) {
                                break;
                            }
                        }
                    }
                    readEntries();
                } else {
                    --remaining_dir_entries;
                    if (remaining_dir_entries == 0 && finished) {
                        finished(dirEntry);
                    }
                }
            }, errorHandler);
        };

        // Start reading dirs
        ++remaining_dir_entries;
        readEntries();
    }
}

function startGameHandler(element) {
    item = element.target.getAttribute("data-name");
    currentFolderName = gameEntries[item].fullPath;

    loadDirEntry(gameEntries[item], 2, function(i) {
        fsEntries[i.fullPath] = i;
        return true;
    });

    startGame();
}

function addGame(dirEntry) {
    gameEntries[dirEntry.fullPath] = dirEntry;

    var elem = document.createElement("div");
    gamelist.appendChild(elem);
    elem.setAttribute("data-name", dirEntry.fullPath);

    var img = document.createElement("img");
    img.src = '/assets/notitle.png';
    img.setAttribute("data-name", dirEntry.fullPath);
    elem.appendChild(img);
    elem.onclick = startGameHandler;

    var txt = document.createElement("div");
    txt.setAttribute("data-name", dirEntry.fullPath);
    txt.innerHTML = dirEntry.name;
    elem.appendChild(txt);

    dirEntry.getDirectory('Title', {}, function(titleEntry) {
        loadDirEntry(titleEntry, 0, function(item) {
            if (!item.isDirectory) {
                var name = item.name.toLowerCase();
                if (name.endsWith(".xyz") || name.endsWith(".bmp") || name.endsWith(".png")) {
                    readFileToArrayBuffer(item, function(res) {
                        var img = document.createElement("img");
                        var arr = undefined;

                        if (name.endsWith(".xyz")) {
                            var xyz = Module.xyz2png;

                            var stream = xyz.FS.open(name, "w");
                            xyz.FS.write(stream, new Uint8Array(res), 0, res.byteLength, 0);
                            xyz.FS.close(stream);

                            if (xyz.invoke(name) == 0) {
                                arr = xyz.FS.readFile(name.substring(0, name.lastIndexOf(".")) + ".png");
                            }
                        } else {
                            var arr = new Uint8Array(res);
                        }

                        img = elem.querySelector("img");
                        if (arr !== undefined) {
                            img.src = 'data:image/png;base64,' + uint8ArrayToBase64(arr);
                        }
                    });
                    return false;
                }
            }
            return true;
        });
    });
}

function searchForGames(theEntry) {
    var entries = [];
    loadDirEntry(theEntry, 3, function(item, dirEntry) {
        if (item.name.toLowerCase() == "rpg_rt.ldb") {
            entries.push(dirEntry);
            return false;
        }
        return true;
    }, function() {
        entries.sort(function(a, b) {
            return a.fullPath.localeCompare(b.fullPath);
        });
        entries.forEach(function(x) {
            addGame(x);
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

        removeChildren(gamelist);
        searchForGames(gameBrowserEntry);
    });
});

website.addEventListener('click', function(e) {
    // Needs Chrome 42
    chrome.browser.openTab({"url": "https://easyrpg.org"});
    e.preventDefault();
});

document.querySelector('#fs_button').addEventListener("click", function() {
    // Fullscreen handling
    Browser.requestFullScreen();
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
                    searchForGames(gameBrowserEntry);
                });
            });

            // Show game browser
            frontend.style.visibility = "visible";
        }
    }
};
xhr.send(null)
