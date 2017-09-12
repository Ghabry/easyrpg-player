// Removes the non-game folders
function removeFolderPrefix(s) {
    return s.substring(currentFolderName.length + 1)
}

function replaceAll(str, find, replace) {
    return str.replace(new RegExp(find, 'g'), replace);
}

function uint8ArrayToBase64(array) {
    var binary = '';
    var len = array.length;
    for (var i = 0; i < len; i++) {
        binary += String.fromCharCode(array[i]);
    }
    return window.btoa(binary);
}

function readFileToArrayBuffer(fileEntry, callback) {
    fileEntry.file(function(file) {
        var reader = new FileReader();

        reader.onerror = errorHandler;
        reader.onload = function(e) {
            callback(e.target.result);
        };

        reader.readAsArrayBuffer(file);
    });
}

function removeChildren(node) {
    while (node.firstChild) {
        node.removeChild(node.firstChild);
    }
}

function startGame() {
    // Launch the Player
    var script = document.createElement('script');
    script.src = "player/index.js";
    document.body.appendChild(script);

    // Show the Player
    player.style.visibility = "visible";
}
