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

// Hide both divs on startup
frontend.style.visibility = "hidden";
player.style.visibility = "hidden";

// standard error handler
function errorHandler(e) {
    console.error(e);
}

function loadDirEntry(entry, depth) {
    if (depth > 2) {
        // Prevent deep recursion
        return;
    }

    if (entry.isDirectory) {
        var dirReader = entry.createReader();

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

function startGame() {
    // Launch the Player
    var script = document.createElement('script');
    script.src = "player/index.js";
    document.body.appendChild(script);

    // Show the Player
    player.style.visibility = "visible";
    frontend.style.visibility = "hidden";
}

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

// Test for standalone mode
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
                    loadDirEntry(theEntry, 0);
                });
            });

            // Show game browser
            frontend.style.visibility = "visible";
        }
    }
};
xhr.send(null)
