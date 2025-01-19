package org.easyrpg.player.game_browser;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.util.Base64;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.documentfile.provider.DocumentFile;

import org.easyrpg.player.Helper;
import org.easyrpg.player.settings.SettingsManager;

import java.io.ByteArrayOutputStream;

public class Game implements Comparable<Game> {
    final static char escapeCode = '\u0001';
    final static String cacheVersion = "1";
    /** The title shown in the Game Browser */
    private String title;
    /** Bytes of the title string in an unspecified encoding */
    private byte[] titleRaw = null;
    /** Human readable version of the game directory. Shown in the game browser
     *  when the specific setting is enabled.
     */
    private String gameFolderName;
    /** Path to the game folder (forwarded via --project-path */
    private final String gameFolderPath;
    /** Relative path to the save directory, made absolute by calling createSaveUri */
    private String savePath = "";
    /** Whether the game was tagged as a favourite */
    private boolean isFavorite;
    /** Title image shown in the Game Browser */
    private Bitmap titleScreen = null;
    /** Game is launched from the APK via standalone mode */
    private boolean standalone = false;

    public Game(String gameFolderPath, String saveFolder, byte[] titleScreen) {
        this.gameFolderPath = gameFolderPath;

        // is only relative here, launchGame will put this in the "saves" directory
        if (!saveFolder.isEmpty()) {
            savePath = saveFolder;
        }

        if (titleScreen != null) {
            this.titleScreen = BitmapFactory.decodeByteArray(titleScreen, 0, titleScreen.length);
        };

        this.isFavorite = isFavoriteFromSettings();
    }

    public String getDisplayTitle() {
        String customTitle = getCustomTitle();
        if (!customTitle.isEmpty()) {
            return customTitle;
        }

        if (SettingsManager.getGameBrowserLabelMode() == 0) {
            return getTitle();
        } else {
            return gameFolderName;
        }
    }

    public String getTitle() {
        return title;
    }

    public void setTitle(String title) {
        this.title = title;
    }

    public native void reencodeTitle();

    public String getCustomTitle() {
        return SettingsManager.getCustomGameTitle(this);
    }

    public void setCustomTitle(String customTitle) {
        SettingsManager.setCustomGameTitle(this, customTitle);
    }

    public String getGameFolderPath() {
        return gameFolderPath;
    }

    public String getSavePath() {
        return savePath;
    }

    public void setSavePath(String path) {
        savePath = path;
    }

    public String getGameFolderName() {
        return gameFolderName;
    }

    public void setGameFolderName(String gameFolderName) {
        this.gameFolderName = gameFolderName;
    }

    public boolean isFavorite() {
        return isFavorite;
    }

    public void setFavorite(boolean isFavorite) {
        this.isFavorite = isFavorite;
        if(isFavorite){
            SettingsManager.addFavoriteGame(this);
        } else {
            SettingsManager.removeAFavoriteGame(this);
        }
    }

    private boolean isFavoriteFromSettings() {
        return SettingsManager.getFavoriteGamesList().contains(this.getKey());
    }

    @Override
    public int compareTo(Game game) {
        if (this.isFavorite() && !game.isFavorite()) {
            return -1;
        }
        if (!this.isFavorite() && game.isFavorite()) {
            return 1;
        }
        return this.getDisplayTitle().compareTo(game.getDisplayTitle());
    }

    /**
     * Returns a unique key to be used for storing settings related to the game.
     *
     * @return unique key
     */
    public String getKey() {
        return gameFolderPath.replaceAll("[/ ]", "_");
    }

    public Encoding getEncoding() {
        return SettingsManager.getGameEncoding(this);
    }

    public void setEncoding(Encoding encoding) {
        SettingsManager.setGameEncoding(this, encoding);
        reencodeTitle();
    }

    /**
     * @return The encoding number or "auto" when not configured (for use via JNI)
     */
    public String getEncodingCode() {
        return getEncoding().getRegionCode();
    }

    public Bitmap getTitleScreen() {
        return titleScreen;
    }

    public boolean isStandalone() {
        return standalone;
    }

    public void setStandalone(boolean standalone) {
        this.standalone = standalone;
    }

    @NonNull
    @Override
    public String toString() {
        return getDisplayTitle();
    }

    public Uri createSaveUri(Context context) {
        if (!getSavePath().isEmpty()) {
            DocumentFile saveFolder = Helper.createFolderInSave(context, getSavePath());

            if (saveFolder != null) {
                return saveFolder.getUri();
            }
        } else {
            return Uri.parse(getGameFolderPath());
        }

        return null;
    }

    public static Game fromCacheEntry(Context context, String cache) {
        String[] entries = cache.split(String.valueOf(escapeCode));

        if (entries.length != 7 || !entries[0].equals(cacheVersion)) {
            return null;
        }

        String savePath = entries[1];
        DocumentFile gameFolder = DocumentFile.fromTreeUri(context, Uri.parse(entries[2]));
        if (gameFolder == null) {
            return null;
        }

        String gameFolderName = entries[3];

        String title = entries[4];

        byte[] titleRaw = null;
        if (!entries[5].equals("null")) {
            titleRaw = Base64.decode(entries[5], 0);
        }

        byte[] titleScreen = null;
        if (!entries[6].equals("null")) {
            titleScreen = Base64.decode(entries[6], 0);
        }

        Game g = new Game(entries[2], savePath, titleScreen);
        g.setTitle(title);
        g.titleRaw = titleRaw;

        if (g.titleRaw != null) {
            g.reencodeTitle();
        }

        g.setGameFolderName(gameFolderName);

        return g;
    }

    public String toCacheEntry() {
        StringBuilder sb = new StringBuilder();

        // Cache structure: savePath | gameFolderPath | title | titleRaw | titleScreen
        sb.append(cacheVersion); // 0
        sb.append(escapeCode);
        sb.append(savePath); // 1
        sb.append(escapeCode);
        sb.append(gameFolderPath); // 2
        sb.append(escapeCode);
        sb.append(gameFolderName); // 3
        sb.append(escapeCode);
        sb.append(title); // 4
        sb.append(escapeCode);
        if (titleRaw != null) { // 5
            sb.append(Base64.encodeToString(titleRaw, Base64.NO_WRAP));
        } else {
            sb.append("null");
        }
        sb.append(escapeCode);
        if (titleScreen != null) { // 6
            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            titleScreen.compress(Bitmap.CompressFormat.PNG, 90, baos);
            byte[] b = baos.toByteArray();
            sb.append(Base64.encodeToString(b, Base64.NO_WRAP));
        } else {
            sb.append("null");
        }

        return sb.toString();
    }

}
