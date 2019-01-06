/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EP_FILEFINDER_H
#define EP_FILEFINDER_H

// Headers
#include "system.h"
#include "filesystem.h"
#include <string>
#include <cstdio>
#include <vector>

#ifdef PSP2
#  include <psp2/types.h>
#  define Offset SceOff
#else
#  define Offset off_t
#endif

/**
 * FileFinder contains helper methods for finding case
 * insensitive files paths.
 */
namespace FileFinder {
	/**
	 * Quits FileFinder.
	 */
	void Quit();

	/**
	 * Finds an image file.
	 * Searches through the current RPG Maker game and the RTP directories.
	 *
	 * @param dir directory to check.
	 * @param name image file name to check.
	 * @return path to file.
	 */
	std::string FindImage(const std::string& dir, const std::string& name);

	/**
	 * Finds a file.
	 * Searches through the current RPG Maker game and the RTP directories.
	 *
	 * @param dir directory to check.
	 * @param name file name to check.
	 * @return path to file.
	 */
	std::string FindDefault(const std::string& dir, const std::string& name);

	/**
	 * Finds a file.
	 * Searches through the current RPG Maker game and the RTP directories.
	 *
	 * @param name the path and name.
	 * @return path to file.
	 */
	std::string FindDefault(const std::string& name);

	/**
	 * Finds a music file.
	 * Searches through the Music folder of the current RPG Maker game and
	 * the RTP directories.
	 *
	 * @param name the music path and name.
	 * @return path to file.
	 */
	std::string FindMusic(const std::string& name);

	/**
	 * Finds a sound file.
	 * Searches through the Sound folder of the current RPG Maker game and
	 * the RTP directories.
	 *
	 * @param name the sound path and name.
	 * @return path to file.
	 */
	std::string FindSound(const std::string& name);

	/**
	 * Finds a font file.
	 * Searches through the current RPG Maker game and the RTP directories.
	 *
	 * @param name the font name.
	 * @return path to file.
	 */
	std::string FindFont(const std::string& name);

	/**
	 * Creates stream from UTF-8 file name for reading.
	 *
	 * @param name UTF-8 string file name.
	 * @param m stream mode.
	 * @return NULL if open failed.
	 */
	std::shared_ptr<std::istream> OpenInputStream(const std::string &name, std::ios_base::openmode m = (std::ios_base::openmode)0);

	/**
	 * Checks whether passed file is directory.
	 * This function is case sensitive on some platforms.
	 *
	 * @param file file to check.
	 * @return true if file is directory, otherwise false.
	 */
	bool IsDirectory(const std::string& file);

	/**
	 * Checks whether passed file exists.
	 * This function is case sensitive on some platforms.
	 *
	 * @param file file to check
	 * @return true if file exists, otherwise false.
	 */
	bool Exists(const std::string& file);

	/**
	 * Return the part of "path_in" that is inside the current games directory.
	 *
	 * @see GetPathInsidePath
	 * @param path_in An absolute path inside the game directory
	 * @return The part of path_in that is inside the game directory, path_in when it's not in the directory
	 */
	std::string GetPathInsideGamePath(const std::string& path_in);

	/**
	 * Gets the virtual filesystem that is used by the current game.
	 *
	 * @return Handle to the game filesystem
	 */
	const FilesystemRef GetGameFilesystem();

	/**
	 * Sets the virtual filesystem used for executing the current RPG Maker
	 * game.
	 *
	 * @param filesystem game filesystem
	 */
	void SetGameFilesystem(FilesystemRef filesystem);

	/**
	 * Gets the virtual filesystem used for all write operations.
	 *
	 * @return Handle to the save filesystem
	 */
	const FilesystemRef GetSaveFilesystem();

	/**
	 * Sets the virtual filesystem used for all write operations.
	 *
	 * @param filesystem save filesystem
	 */
	void SetSaveFilesystem(FilesystemRef filesystem);

	/**
	 * Returns a filesystem which can access any path on the host.
	 * Any file access is passed through which means any global and
	 * local path will work as expected.
	 *
	 * @return Handle to the host filesystem
	 */
	FilesystemRef GetNativeFilesystem();

	/**
	 * Checks whether the save directory contains any savegame with name
	 * SaveXX.lsd (XX from 00 to 15).
	 *
	 * @return If directory tree contains a savegame
	 */
	bool HasSavegame();

	/**
	 * Get the size of a file
	 *
	 * @param file the path to a file
	 * @return the filesize, or -1 on error
	 */
	Offset GetFileSize(const std::string& file);

	/**
	 * Known file sizes
	 */
	enum KnownFileSize {
		OFFICIAL_HARMONY_DLL = 473600,
	};

	bool IsValidProject();
	bool IsValidProject(FilesystemRef fs);
	bool IsRPG2kProject(FilesystemRef fs);
	bool IsEasyRpgProject(FilesystemRef fs);

	/**
	 * Checks whether the game is created with RPG2k >= 1.50 or RPG2k3 >= 1.05.
	 *
	 * @return true if RPG2k >= 1.50 or RPG2k3 >= 1.05, otherwise false.
	 */
	bool IsMajorUpdatedTree();

	/** RPG_RT.exe file size thresholds
         *
         * 2k v1.51 (Japanese)    : 746496
         * 2k v1.50 (Japanese)    : 745984
         *  -- threshold (2k) --  : 735000
         * 2k v1.10 (Japanese)    : 726016
         *
         * 2k3 v1.09a (Japanese)  : 950784
         * 2k3 v1.06 (Japanese)   : 949248
         * 2k3 v1.05 (Japanese)   : unknown
         *  -- threshold (2k3) -- : 927000
         * 2k3 v1.04 (Japanese)   : 913408
         */
	enum RpgrtMajorUpdateThreshold {
		RPG2K = 735000,
		RPG2K3 = 927000,
	};
} // namespace FileFinder

#endif
