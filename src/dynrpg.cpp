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
#include "utils.h"

#include <map>
#include <sstream>
#include <string>

enum DynRpg_ParseMode {
	ParseMode_Function,
	ParseMode_WaitForComma,
	ParseMode_WaitForArg,
	ParseMode_String,
	ParseMode_Token
};

typedef std::vector<std::string> dyn_arg_list;
typedef bool(*dynfunc)(dyn_arg_list);
typedef std::map<std::string, dynfunc> dyn_rpg_func;

namespace {
	// already reported unknown funcs
	std::map<std::string, int> unknown_functions;

	float GetFloat(std::string str, bool* valid = NULL) {
		std::istringstream iss(str);
		float f;
		iss >> f;

		if (valid) {
			*valid = iss.eof() && !iss.fail();
		}

		return f;
	}

	// Var arg referenced by $n
	std::string ParseVarArg(const dyn_arg_list &args, int index) {
		if (index >= args.size()) {
			return "";
		}

		std::u32string::iterator text_index, end;
		std::u32string text = Utils::DecodeUTF32(args[index]);
		text_index = text.begin();
		end = text.end();

		std::stringstream msg;

		for (; text_index != end; ++text_index) {
			char32_t chr = *text_index;

			// Test for "" -> append "
			// otherwise end of string
			if (chr == '$' && std::distance(text_index, end) > 1) {
				char32_t n = *std::next(text_index, 1);

				if (n == '$') {
					// $$ = $
					msg << n;
					++text_index;
				} else if (n >= '1' && n <= '9') {
					int i = (int)(n - '0');

					if (i + index < args.size()) {
						msg << args[i + index];
					}
					else {
						// $-ref out of range
						return "";
					}

					++text_index;
				} else {
					msg << chr;
				}
			} else {
				msg << chr;
			}
		}

		return msg.str();
	}

	// Macros

#define DYNRPG_FUNCTION(var) \
	std::string func_name = var;

#define DYNRPG_CHECK_ARG_LENGTH(len) \
	if (args.size() != len) {\
	Output::Warning("%s: Got %d args (needs %d)", func_name.c_str(), args.size(), len); \
	return true; \
	}

#define DYNRPG_CHECK_ARG_MIN(len) \
	if (args.size() < len) { \
		Output::Warning("%s: Got %d args (needs %d or more)", func_name.c_str(), args.size(), len); \
		return true; \
	}

#define DYNRPG_GET_FLOAT_ARG(i, var) \
	float var; \
	bool valid_float##var; \
	var = GetFloat(args[i], &valid_float##var); \
	if (!valid_float##var) { \
	Output::Warning("%s: Arg %d (%s) is not numeric", func_name.c_str(), i, args[i].c_str()); \
	return true; \
	}

#define DYNRPG_GET_INT_ARG(i, var) \
	DYNRPG_GET_FLOAT_ARG(i, var##_float_arg) \
	int var = (int)var##_float_arg;

#define DYNRPG_GET_STR_ARG(i, var) \
	std::string& var = args[i];

#define DYNRPG_GET_VAR_ARG(i, var) \
	std::string var = ParseVarArg(args, i); \
	if (var.empty()) { \
	Output::Warning("%s: Vararg %d out of range", func_name.c_str(), i); \
	}

	// DynRpg Functions

	bool Add(dyn_arg_list args) {
		DYNRPG_FUNCTION("add");

		DYNRPG_CHECK_ARG_LENGTH(2);

		DYNRPG_GET_INT_ARG(0, arg1);
		DYNRPG_GET_INT_ARG(1, arg2);

		Game_Variables[arg1] += arg2;

		return true;
	}

	bool Sub(dyn_arg_list args) {
		DYNRPG_FUNCTION("sub");

		DYNRPG_CHECK_ARG_LENGTH(2);

		DYNRPG_GET_INT_ARG(0, arg1);
		DYNRPG_GET_INT_ARG(1, arg2);

		Game_Variables[arg1] -= arg2;

		return true;
	}

	bool Mul(dyn_arg_list args) {
		DYNRPG_FUNCTION("mul");

		DYNRPG_CHECK_ARG_LENGTH(2);

		DYNRPG_GET_INT_ARG(0, arg1);
		DYNRPG_GET_INT_ARG(1, arg2);

		Game_Variables[arg1] *= arg2;

		return true;
	}

	bool Div(dyn_arg_list args) {
		DYNRPG_FUNCTION("div");

		DYNRPG_CHECK_ARG_LENGTH(2);

		DYNRPG_GET_INT_ARG(0, arg1);
		DYNRPG_GET_INT_ARG(1, arg2);

		Game_Variables[arg1] /= arg2;

		return true;
	}

	bool Mod(dyn_arg_list args) {
		DYNRPG_FUNCTION("mod");

		DYNRPG_CHECK_ARG_LENGTH(2);

		DYNRPG_GET_INT_ARG(0, arg1);
		DYNRPG_GET_INT_ARG(1, arg2);

		Game_Variables[arg1] %= arg2;

		return true;
	}

	bool Oput(dyn_arg_list args) {
		DYNRPG_FUNCTION("output")

		DYNRPG_CHECK_ARG_MIN(2);
		
		DYNRPG_GET_STR_ARG(0, mode);
		DYNRPG_GET_VAR_ARG(1, msg);

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

	// Function table

	dyn_rpg_func const dyn_rpg_functions = {
			{"add", Add},
			{"sub", Sub},
			{"mul", Mul},
			{"div", Div},
			{"mod", Mod},
			{"output", Oput}};
}

static std::string ParseToken(const std::string& token, const std::string& function_name) {
	std::u32string::iterator text_index, end;
	std::u32string text = Utils::DecodeUTF32(token);
	text_index = text.begin();
	end = text.end();

	char32_t chr = *text_index;

	bool first = true;

	bool number_encountered = false;

	std::stringstream var_part;
	std::stringstream number_part;

	for (;;) {
		if (text_index != end) {
			chr = *std::next(text_index, 1);
		}

		if (text_index == end) {
			// Variable reference
			std::string tmp = number_part.str();
			int number = atoi(tmp.c_str());
			tmp = var_part.str();

			// Convert backwards
			for (std::string::reverse_iterator it = tmp.rbegin(); it != tmp.rend(); ++it) {
				if (*it == 'N') {
					if (!Game_Actors::ActorExists(number)) {
						Output::Warning("%s: Invalid actor id %d in %s", function_name.c_str(), number, token.c_str());
						return "";
					}

					// N is last
					return Game_Actors::GetActor(number)->GetName();
				} else {
					// Variable
					if (!Game_Variables.IsValid(number)) {
						Output::Warning("%s: Invalid variable %d in %s", function_name.c_str(), number, token.c_str());
						return "";
					}

					number = Game_Variables[number];
				}
			}

			number_part.str("");
			number_part << number;
			return number_part.str();
		} else if (chr == 'N') {
			if (!first || number_encountered) {
				break;
			}
			var_part << chr;
		} else if (chr == 'V') {
			if (number_encountered) {
				break;
			}
			var_part << chr;
		}
		else if (chr >= '0' && chr <= '9') {
			number_encountered = true;
			number_part << chr;
		} else {
			break;
		}

		++text_index;
		first = false;
	}

	// Normal token
	return token;
}

static bool ValidFunction(const std::string& token) {
	if (token.empty()) {
		// empty function name
		return false;
	}

	if (dyn_rpg_functions.find(token) == dyn_rpg_functions.end()) {
		// Not a supported function
		// Avoid spamming by reporting only once per function
		if (unknown_functions.find(token) == unknown_functions.end()) {
			unknown_functions[token] = 1;
			Output::Warning("Unsupported DynRPG function: %s", token.c_str());
		}
		return false;
	}

	return true;
}

bool DynRpg::Invoke(RPG::EventCommand const& com) {
	if (com.string.empty()) {
		// Not a DynRPG function (empty comment)
		return true;
	}

	std::u32string::iterator text_index, end;
	std::u32string text = Utils::DecodeUTF32(com.string);
	text_index = text.begin();
	end = text.end();

	char32_t chr = *text_index;

	if (chr != '@') {
		// Not a DynRPG function, normal comment
		return true;
	}

	DynRpg_ParseMode mode = ParseMode_Function;
	std::string function_name;
	std::string tmp;
	std::u32string u32_tmp;
	dyn_arg_list args;
	std::stringstream token;

	// Parameters can be of type Token, Number or String
	// Strings are in "", a "-literal is represented by ""
	// Number is a valid float number
	// Tokens are Strings without "" and with Whitespace stripped o_O
	// If a token is (regex) N?V+[0-9]+ it is resolved to a var or an actor
	
	// All arguments are passed as string to the DynRpg functions and are
	// converted to int or float on demand.
	
	for (;;) {
		if (text_index != end) {
			chr = *std::next(text_index, 1);
		}

		if (text_index == end) {
			switch (mode) {
			case ParseMode_Function:
				// End of function token
				ValidFunction(token.str());
				function_name = Utils::LowerCase(token.str());
				token.str("");

				mode = ParseMode_WaitForArg;
				break;
			case ParseMode_WaitForComma:
				// no-op
				break;
			case ParseMode_WaitForArg:
				if (args.size() > 0) {
					// Found , but no token -> empty arg
					args.push_back("");
				}
				break;
			case ParseMode_String:
				Output::Warning("%s: Unterminated literal", function_name.c_str());
				return true;
			case ParseMode_Token:
				tmp = ParseToken(token.str(), function_name);
				if (tmp.empty()) {
					return true;
				}
				args.push_back(tmp);
				mode = ParseMode_WaitForComma;
				token.str("");
				break;
			}

			break;
		} else if (chr == ' ') {
			switch (mode) {
			case ParseMode_Function:
				// End of function token
				ValidFunction(token.str());
				function_name = Utils::LowerCase(token.str());
				token.str("");

				mode = ParseMode_WaitForArg;
				break;
			case ParseMode_WaitForComma:
			case ParseMode_WaitForArg:
				// no-op
				break;
			case ParseMode_String:
				u32_tmp = chr;
				token << Utils::EncodeUTF(u32_tmp);
				break;
			case ParseMode_Token:
				// Skip whitespace
				break;
			}
		} else if (chr == ',') {
			switch (mode) {
			case ParseMode_Function:
				// End of function token
				Output::Warning("%s: Expected space or end, got \",\"", function_name.c_str());
				return true;
			case ParseMode_WaitForComma:
				mode = ParseMode_WaitForArg;
				break;
			case ParseMode_WaitForArg:
				// Empty arg
				args.push_back("");
				break;
			case ParseMode_String:
				u32_tmp = chr;
				token << Utils::EncodeUTF(u32_tmp);
				break;
			case ParseMode_Token:
				tmp = ParseToken(token.str(), function_name);
				if (tmp.empty()) {
					return true;
				}
				args.push_back(tmp);
				// already on a comma
				mode = ParseMode_WaitForArg;
				token.str("");
				break;
			}
		} else {
			// Anything else that isn't special purpose
			switch (mode) {
			case ParseMode_Function:
				u32_tmp = chr;
				token << Utils::EncodeUTF(u32_tmp);
				break;
			case ParseMode_WaitForComma:
				Output::Warning("%s: Expected \",\", got token", function_name.c_str());
				return true;
			case ParseMode_WaitForArg:
				if (chr == '"') {
					mode = ParseMode_String;
					// begin of string
				}
				else {
					mode = ParseMode_Token;
					token << chr;
				}
				break;
			case ParseMode_String:
				if (chr == '"') {
					// Test for "" -> append "
					// otherwise end of string
					if (std::distance(text_index, end) > 1 && *std::next(text_index, 2) == '"') {
						token << '"';
						++text_index;
					}
					else {
						// End of string
						args.push_back(token.str());

						mode = ParseMode_WaitForComma;
						token.str("");
					}
				}
				else {
					u32_tmp = chr;
					token << Utils::EncodeUTF(u32_tmp);
				}
				break;
			case ParseMode_Token:
				u32_tmp = chr;
				token << Utils::EncodeUTF(u32_tmp);
				break;
			}
		}

		++text_index;
	}

	dyn_rpg_func::const_iterator const name_it = dyn_rpg_functions.find(function_name);

	if (name_it != dyn_rpg_functions.end()) {
		return name_it->second(args);
	}
	return true;
}
