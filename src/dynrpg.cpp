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

// Headers
#include "dynrpg.h"
#include "game_actors.h"
#include "game_variables.h"
#include "output.h"
#include "player.h"
#include "rpg_eventcommand.h"

#include <boost/assign/list_of.hpp>
#include <boost/regex/pending/unicode_iterator.hpp>
#include <map>
#include <sstream>
#include <string>

using boost::assign::map_list_of;

enum DynRpg_ArgType {
	ArgType_String,
	ArgType_Number
};

enum DynRpg_ParseMode {
	ParseMode_Function,
	ParseMode_WaitForComma,
	ParseMode_WaitForArg,
	ParseMode_String,
	ParseMode_Token
};

typedef struct {
	DynRpg_ArgType type;
	float float_val;
	std::string str_val;
} dyn_arg;

typedef std::vector<dyn_arg> dyn_arg_list;

typedef bool(*dynfunc)(dyn_arg_list);
typedef std::map<std::string, dynfunc> dyn_rpg_func;
typedef boost::u8_to_u32_iterator<std::string::const_iterator> u8_to_u32;
typedef boost::u32_to_u8_iterator<u8_to_u32> u32_to_u8;

namespace {
	bool IsFloat(std::string str) {
		std::istringstream iss(str);
		float f;
		iss >> f;
		return iss.eof() && !iss.fail();
	}

#define DYNRPG_CHECK_ARG_LENGTH(len) \
	if (args.size() != len) { return true; }

#define DYNRPG_GET_FLOAT_ARG(i, var) \
	float var; \
	if (args[i].type != ArgType_Number) { return true; } \
	else { var = args[i].float_val; }

#define DYNRPG_GET_INT_ARG(i, var) \
	DYNRPG_GET_FLOAT_ARG(i, var##_float_arg) \
	int var = (int)var##_float_arg;

#define DYNRPG_GET_STR_ARG(i, var) \
	std::string var; \
	if (args[i].type != ArgType_String) { std::stringstream ss; ss << args[i].float_val; var = ss.str(); } \
	else { var = args[i].str_val; }

	u8_to_u32 text_index, end;

	bool Add(dyn_arg_list args) {
		DYNRPG_CHECK_ARG_LENGTH(2);

		DYNRPG_GET_INT_ARG(0, arg1);
		DYNRPG_GET_INT_ARG(1, arg2);

		Game_Variables[arg1] += arg2;

		return true;
	}

	bool Sub(dyn_arg_list args) {
		DYNRPG_CHECK_ARG_LENGTH(2);

		DYNRPG_GET_INT_ARG(0, arg1);
		DYNRPG_GET_INT_ARG(1, arg2);

		Game_Variables[arg1] -= arg2;

		return true;
	}

	bool Mul(dyn_arg_list args) {
		DYNRPG_CHECK_ARG_LENGTH(2);

		DYNRPG_GET_INT_ARG(0, arg1);
		DYNRPG_GET_INT_ARG(1, arg2);

		Game_Variables[arg1] *= arg2;

		return true;
	}

	bool Div(dyn_arg_list args) {
		DYNRPG_CHECK_ARG_LENGTH(2);

		DYNRPG_GET_INT_ARG(0, arg1);
		DYNRPG_GET_INT_ARG(1, arg2);

		Game_Variables[arg1] /= arg2;

		return true;
	}

	bool Mod(dyn_arg_list args) {
		DYNRPG_CHECK_ARG_LENGTH(2);

		DYNRPG_GET_INT_ARG(0, arg1);
		DYNRPG_GET_INT_ARG(1, arg2);

		Game_Variables[arg1] %= arg2;

		return true;
	}

	bool Exit(dyn_arg_list args) {
		DYNRPG_CHECK_ARG_LENGTH(0);

		Player::exit_flag = true;

		return true;
	}

	bool Oput(dyn_arg_list args) {
		DYNRPG_CHECK_ARG_LENGTH(2);

		DYNRPG_GET_STR_ARG(0, mode);
		DYNRPG_GET_STR_ARG(1, msg);

		if (mode == "Debug") {
			Output::DebugStr(msg);
		} else if (mode == "Info") {
			Output::PostStr(msg);
		} else if (mode == "Warning") {
			Output::WarningStr(msg);
		} else if (mode == "Error") {
			Output::ErrorStr(msg);
		}

		return true;
	}

	dyn_rpg_func const dyn_rpg_functions = map_list_of
		("add", Add)
		("sub", Sub)
		("mul", Mul)
		("div", Div)
		("mod", Mod)
		("exit", Exit)
		("output", Oput);
}

bool DynRpg::Invoke(RPG::EventCommand const& com) {
	if (com.string.empty()) {
		return true;
	}

	text_index = u8_to_u32(com.string.begin(), com.string.begin(), com.string.end());
	end = u8_to_u32(com.string.end(), com.string.begin(), com.string.end());

	std::string func_name;

	std::string chr = std::string(u32_to_u8(text_index), u32_to_u8(boost::next(text_index, 1)));

	if (chr != "@") {
		return true;
	}

	++text_index;

	DynRpg_ParseMode mode = ParseMode_Function;
	std::string function_name;
	dyn_arg arg;
	dyn_arg_list args;
	std::stringstream str;
	std::string tmp;

	// If a token could be a indirect reference to a variable/actor:
	// [VN]+[0-9]+
	bool var_actor_ref = true;
	std::stringstream var;
	std::stringstream num;
	
	for (;;) {
		if (std::distance(text_index, end) >= 1) {
			chr = std::string(u32_to_u8(text_index), u32_to_u8(boost::next(text_index, 1)));
		}
		else {
			chr = "";
		}

		// Test for end of item (except if in string)
		if (text_index == end || chr == " " || chr == ",") {
			switch (mode) {
			case ParseMode_Function:
				function_name = str.str();

				if (function_name.empty()) {
					return true;
				}

				if (dyn_rpg_functions.find(function_name) == dyn_rpg_functions.end()) {
					return true;
				}

				mode = ParseMode_WaitForArg;
				str.str("");
				break;
			case ParseMode_WaitForComma:
				if (chr == ",") {
					mode = ParseMode_WaitForArg;
					str.str("");
				}
				break;
			case ParseMode_WaitForArg:
				// no-op
				break;
			case ParseMode_String:
				if (text_index == end) {
					// Unterminated literal
					return true;
				}

				str << chr;
				break;
			case ParseMode_Token:
				if (chr != " ") {
					// For some reason whitespaces in tokens must be stripped

					tmp = str.str();

					if (var_actor_ref && var.str().length() > 0 && num.str().length() > 0) {
						tmp = num.str();
						int number = atoi(tmp.c_str());

						// Convert backwards
						tmp = var.str();
						for (std::string::reverse_iterator it = tmp.rbegin(); it != tmp.rend(); ++it) {
							if (*it == 'N') {
								if (!Game_Actors::ActorExists(number)) {
									return false;
								}

								arg.type = ArgType_String;
								arg.str_val = Game_Actors::GetActor(number)->GetName();
								args.push_back(arg);
								tmp = "";

								// N is last
								break;
							}
							else {
								// Variable
								if (!Game_Variables.isValidVar(number)) {
									return false;
								}

								number = Game_Variables[number];
							}
						}

						if (!tmp.empty()) {
							arg.type = ArgType_Number;
							arg.float_val = (float)number;
							args.push_back(arg);
						}
					}
					else if (IsFloat(tmp)) {
						arg.type = ArgType_Number;
						arg.float_val = (float)atof(tmp.c_str());
						args.push_back(arg);
					}
					else {
						tmp = str.str();
						arg.type = ArgType_String;
						arg.str_val = tmp;
						args.push_back(arg);
					}
					break;
				}
			};
			
			if (mode != ParseMode_WaitForArg && mode != ParseMode_String) {
				mode = chr == "," ? ParseMode_WaitForArg : ParseMode_WaitForComma;

				str.str("");
				var.str("");
				num.str("");
				var_actor_ref = true;
			}

			if (text_index == end) {
				break;
			}
			
			++text_index;

			continue;
		}

		// Neither space nor comma
		switch (mode) {
		case ParseMode_Function:
			str << chr;
			break;
		case ParseMode_WaitForComma:
			// Parser error
			return true;
		case ParseMode_WaitForArg:
			if (chr == "\"") {
				mode = ParseMode_String;
				// skip "
			}
			else {
				mode = ParseMode_Token;
			}
			break;
		case ParseMode_String:
			if (chr == "\"") {
				// Test for "" -> append "
				// otherwise end of string
				if (std::distance(text_index, end) > 1 && std::string(u32_to_u8(text_index), u32_to_u8(boost::next(text_index, 1))) == "\"") {
					str << '"';
					++text_index;
				}
				else {
					// End of string
					arg.type = ArgType_String;
					arg.str_val = str.str();
					args.push_back(arg);

					mode = ParseMode_WaitForComma;
					str.str("");
				}
			} else {
				str << chr;
			}
			break;
		default:;
		}

		if (mode == ParseMode_Token) {
			str << chr;

			if (var_actor_ref) {
				if (chr == "N" || chr == "V") {
					if (num.str().length() > 0) {
						// Already numbers found -> not a ref
						var_actor_ref = false;
					}
					else if (chr == "N" && var.str().length() > 0) {
						// N must be first
						var_actor_ref = false;
					}
					var << chr;
				}
				else if (chr.length() == 1 && chr[0] >= '0' && chr[0] <= '9') {
					num << chr;
				}
				else {
					// Not a reference
					var_actor_ref = false;
				}
			}
		}

		++text_index;
	}

	dyn_rpg_func::const_iterator const name_it = dyn_rpg_functions.find(function_name);
	return name_it->second(args);
}
