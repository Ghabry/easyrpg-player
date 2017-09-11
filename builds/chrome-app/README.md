# EasyRPG Player Chrome App

## Setup instructions

[Download this archive](https://ci.easyrpg.org/job/player-js/lastSuccessfulBuild/artifact/player-js.tar.gz) and put index.js and index.html.mem in "player".

## Standalone mode

FIXME: TODO

If you want to bundle your own game put it in the "game" folder. Don't put the root directory in it, "game" should directly contain RPG_RT.ldb, System & co. Last but not least get the gencache tool from [our wiki](https://wiki.easyrpg.org/development/player/web) and run it in the game folder to generate the index.json. Everytime you update the game (add new files) you have to regenerate the index.

Don't forget to edit the manifest.json and to replace the assets.
