/*
 * This file is part of liblcf. Copyright (c) 2020 liblcf authors.
 * https://github.com/EasyRPG/liblcf - https://easyrpg.org
 *
 * liblcf is Free/Libre Open Source Software, released under the MIT License.
 * For the full copyright and license information, please view the COPYING
 * file that was distributed with this source code.
 */

#ifndef LCF_DATA_H
#define LCF_DATA_H

#include <string>
#include <vector>
#include "lcf/rpg/actor.h"
#include "lcf/rpg/skill.h"
#include "lcf/rpg/item.h"
#include "lcf/rpg/enemy.h"
#include "lcf/rpg/troop.h"
#include "lcf/rpg/attribute.h"
#include "lcf/rpg/state.h"
#include "lcf/rpg/terrain.h"
#include "lcf/rpg/animation.h"
#include "lcf/rpg/chipset.h"
#include "lcf/rpg/terms.h"
#include "lcf/rpg/system.h"
#include "lcf/rpg/commonevent.h"
#include "lcf/rpg/class.h"
#include "lcf/rpg/battlecommand.h"
#include "lcf/rpg/battleranimation.h"
#include "lcf/rpg/sound.h"
#include "lcf/rpg/music.h"
#include "lcf/rpg/eventcommand.h"
#include "lcf/rpg/treemap.h"
#include "lcf/rpg/database.h"

namespace lcf {

/**
 * Data namespace
 */
namespace Data {
	/** Database Data (ldb) */
	extern rpg::Database data;
	/** @{ */
	extern DBArray<rpg::Actor>& actors;
	extern DBArray<rpg::Skill>& skills;
	extern DBArray<rpg::Item>& items;
	extern DBArray<rpg::Enemy>& enemies;
	extern DBArray<rpg::Troop>& troops;
	extern DBArray<rpg::Terrain>& terrains;
	extern DBArray<rpg::Attribute>& attributes;
	extern DBArray<rpg::State>& states;
	extern DBArray<rpg::Animation>& animations;
	extern DBArray<rpg::Chipset>& chipsets;
	extern DBArray<rpg::CommonEvent>& commonevents;
	extern rpg::BattleCommands& battlecommands;
	extern DBArray<rpg::Class>& classes;
	extern DBArray<rpg::BattlerAnimation>& battleranimations;
	extern rpg::Terms& terms;
	extern rpg::System& system;
	extern DBArray<rpg::Switch>& switches;
	extern DBArray<rpg::Variable>& variables;
	extern DBArray<rpg::StringVariable>& maniac_string_variables;
	/** @} */

	/** TreeMap (lmt) */
	extern rpg::TreeMap treemap;

	/**
	 * Clears database data.
	 */
	void Clear();
}

} //namespace lcf

#endif
