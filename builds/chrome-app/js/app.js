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
// Element that contains path selected by choose game button
var chooseDirButtonText = document.querySelector('#choose_dir_path');
// Frontend <div>
var frontend = document.querySelector('#frontend');
// Player <div>
var player = document.querySelector('#player');
// Website icon
var website = document.querySelector('#website');
// Game list <div>
var gamelist = document.querySelector('#game-grid');
// Contains the game that was selected in the game browser
var selected_game = undefined;
// Contains the encoding of the running game
var selected_game_encoding = undefined;

var gb_state = {
    "NoGames": 0,
    "Search": 1,
    "Games": 2
};

// Hide both divs on startup
frontend.style.visibility = "hidden";
player.style.visibility = "hidden";

// standard error handler
function errorHandler(e) {
    console.error(e);
}

var remaining_dir_entries = 0;

function loadDirEntry(dirEntry, max_depth, callback, finished) {
    if (dirEntry == undefined) {
        if (remaining_dir_entries == 0 && finished) {
            finished(dirEntry);
        }

        return;
    }

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

function startGameHandler(evt) {
    frontend.remove(); //frontend.style.visibility = "hidden";

    selected_game = evt.target.getAttribute("data-name");
    currentFolderName = gameEntries[selected_game].fullPath;

    getEncodingFor(selected_game, function(e) {
        selected_game_encoding = e;
        loadDirEntry(gameEntries[selected_game], 2, function(i) {
            fsEntries[i.fullPath] = i;
            return true;
        }, function() {
            startGame();
        });
    });
}

function addGame(dirEntry) {
    gameEntries[dirEntry.fullPath] = dirEntry;

    var elem = document.createElement("div");
    elem.classList.add("clickable");
    gamelist.appendChild(elem);
    elem.setAttribute("data-name", dirEntry.fullPath);

    var img = document.createElement("img");
    img.src = '/assets/notitle.png';
    img.setAttribute("data-name", dirEntry.fullPath);
    img.onclick = startGameHandler;
    elem.appendChild(img);

    var txt = document.createElement("div");
    txt.setAttribute("data-name", dirEntry.fullPath);
    txt.innerHTML = dirEntry.name;
    txt.onclick = startGameHandler;
    elem.appendChild(txt);

    var settings = document.createElement("div");
    settings.setAttribute("data-name", dirEntry.fullPath);
    settings.innerHTML = "Settings"
    settings.onclick = openEncodingSettings;
    elem.appendChild(settings);

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

function toggleGameBrowserState(new_state) {
    var game_browser = document.getElementById("game-browser");

    for (var i = 0; i < game_browser.children.length; ++i) {
        game_browser.children[i].classList.add("hidden");
    };

    game_browser.children[new_state].classList.remove("hidden");
}

function searchForGames(theEntry, finish) {
    toggleGameBrowserState(gb_state.Search);

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

        if (entries.length == 0) {
            toggleGameBrowserState(gb_state.NoGames);
        } else {
            toggleGameBrowserState(gb_state.Games);
        }

        entries.forEach(function(x) {
            addGame(x);
        });

        if (finish) {
            finish(entries);
        }
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
        chooseDirButtonText.innerHTML = gameBrowserEntry.fullPath;

        // use local storage to retain access to this file
        chrome.storage.local.set(
            {'gameBrowserDir': chrome.fileSystem.retainEntry(theEntry)}
        );

        removeChildren(gamelist);
        searchForGames(gameBrowserEntry, function(entries) {
            if (entries.length == 1 && entries[0].fullPath == gameBrowserEntry.fullPath) {
                showDialog("You selected a directory that directly contains a game.<br>Please select the parent directory to include all games.");
            } else if (entries.length == 0) {
                showDialog("You selected a directory that doesn't contain any games.<br>See the help page for further information.");
            }
        });
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

/* Encoding handling */
var enc_overlay = document.getElementById("encoding");

function showModal() {
    enc_overlay.classList.remove("hidden");
}

function hideModal() {
    enc_overlay.classList.add("hidden");
}

enc_overlay.addEventListener('click', function(e) {
    if (e.target == enc_overlay) {
        hideModal();
    }
});

function getEncodingFor(name, callback) {
    var key = "encoding/" + name;

    // Retrieve current encoding setting
    chrome.storage.local.get(key, function (result) {
        if (result[key] == undefined) {
            callback("auto");
        } else {
            callback(result[key]);
        }
    });
}

function openEncodingSettings(evt) {
    selected_game = evt.target.getAttribute("data-name");

    // Retrieve current encoding setting
    getEncodingFor(selected_game, function (result) {
        document.querySelectorAll('[data-encoding="' + result + '"]')[0].checked = true;

        showModal();
    });
}

document.getElementById("encoding-ok").addEventListener("click", function(e) {
    var res = document.querySelectorAll('[name=encoding]');
    for (var i = 0; i < res.length; ++i) {
        if (res[i].checked) {
            var key = "encoding/" + selected_game;
            var obj = {}
            obj[key] = res[i].getAttribute("data-encoding");
            chrome.storage.local.set(obj);
            break;
        }
    }

    hideModal();
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
                    chooseDirButtonText.innerHTML = gameBrowserEntry.fullPath;

                    searchForGames(gameBrowserEntry);
                });
            });

            // Show game browser
            frontend.style.visibility = "visible";
        }
    }
};
xhr.send(null)
