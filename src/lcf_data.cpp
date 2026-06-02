/*
 * This file is part of liblcf. Copyright (c) 2020 liblcf authors.
 * https://github.com/EasyRPG/liblcf - https://easyrpg.org
 *
 * liblcf is Free/Libre Open Source Software, released under the MIT License.
 * For the full copyright and license information, please view the COPYING
 * file that was distributed with this source code.
 */

#include "lcf/rpg/database.h"
#include "lcf/data.h"

namespace lcf {

namespace Data {
	rpg::Database data;

	DBArray<rpg::Actor>& actors = data.actors;
	DBArray<rpg::Skill>& skills = data.skills;
	DBArray<rpg::Item>& items = data.items;
	DBArray<rpg::Enemy>& enemies = data.enemies;
	DBArray<rpg::Troop>& troops = data.troops;
	DBArray<rpg::Terrain>& terrains = data.terrains;
	DBArray<rpg::Attribute>& attributes = data.attributes;
	DBArray<rpg::State>& states = data.states;
	DBArray<rpg::Animation>& animations = data.animations;
	DBArray<rpg::Chipset>& chipsets = data.chipsets;
	DBArray<rpg::CommonEvent>& commonevents = data.commonevents;
	rpg::BattleCommands& battlecommands = data.battlecommands;
	DBArray<rpg::Class>& classes = data.classes;
	DBArray<rpg::BattlerAnimation>& battleranimations = data.battleranimations;
	rpg::Terms& terms = data.terms;
	rpg::System& system = data.system;
	DBArray<rpg::Switch>& switches = data.switches;
	DBArray<rpg::Variable>& variables = data.variables;
	DBArray<rpg::StringVariable>& maniac_string_variables = data.maniac_string_variables;

	rpg::TreeMap treemap;
}

void Data::Clear() {
	actors = {};
	skills = {};
	items = {};
	enemies = {};
	troops = {};
	terrains = {};
	attributes = {};
	states = {};
	animations = {};
	chipsets = {};
	commonevents = {};
	battlecommands = {};
	classes = {};
	battleranimations = {};
	terms = {};
	system = {};
	switches = {};
	variables = {};
	maniac_string_variables = {};
	treemap.active_node = 0;
	treemap.maps = {};
	treemap.tree_order = {};
}

} //namespace lcf
