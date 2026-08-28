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

#ifndef EP_CI_UI_H
#define EP_CI_UI_H

// Headers
#include "baseui.h"
#include "audio.h"
#include <memory>

/**
 * CiUi dummy headless UI class for CI mode.
 */
class CiUi final : public BaseUi {
public:
	/**
	 * Constructor.
	 *
	 * @param width window client width.
	 * @param height window client height.
	 * @param cfg config options
	 */
	CiUi(long width, long height, const Game_Config& cfg);

	/**
	 * Destructor.
	 */
	~CiUi() override = default;

	/**
	 * Inherited from BaseUi.
	 */
	/** @{ */
	bool ProcessEvents() override;
	void UpdateDisplay() override;
	void vGetConfig(Game_ConfigVideo& cfg) const override;

	static void ProcessCi();

	struct Config {
		/** Headless execution (use CiUi) */
		bool headless = false;

		/** Enable CI features */
		bool ci_flag = false;

		/** CI output directory name for CI auto-saves/screenshots */
		std::string ci_name;

		/** CI auto-save on each frame */
		bool ci_save = false;
	};
	static inline Config config;

#ifdef SUPPORT_AUDIO
	AudioInterface& GetAudio() override;
#endif
	/** @} */

private:
#ifdef SUPPORT_AUDIO
	std::unique_ptr<EmptyAudio> audio_;
#endif
};

#endif
