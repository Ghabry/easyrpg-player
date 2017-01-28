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
#include <map>

#include "dynrpg_textplugin.h"
#include "baseui.h"
#include "bitmap.h"
#include "drawable.h"
#include "game_actors.h"
#include "game_map.h"
#include "game_party.h"
#include "game_screen.h"
#include "game_variables.h"
#include "graphics.h"
#include "main_data.h"
#include "player.h"

class DynRpgText;

namespace {
	std::map<std::string, std::unique_ptr<DynRpgText>> graphics;
}

class DynRpgText : public Drawable {
public:
	DynRpgText(int pic_id, int x, int y, const std::string& text) : pic_id(pic_id), x(x), y(y) {
		Graphics::RegisterDrawable(this);

		AddLine(text);
	}

	~DynRpgText() {
		Graphics::RemoveDrawable(this);
	};

	void AddLine(const std::string& text) {
		texts.push_back(text);

		Refresh();
	}

	void AddText(const std::string& text) {
		if (texts.empty()) {
			texts.push_back(text);
		} else {
			texts.back() += text;
		}

		Refresh();
	}

	void ClearText() {
		texts.clear();

		Refresh();
	}

	void SetPosition(int new_x, int new_y) {
		x = new_x;
		y = new_y;
	}

	void SetColor(int new_color) {
		color = new_color;

		Refresh();
	}

	void SetPictureId(int new_pic_id) {
		pic_id = new_pic_id;

		Refresh();
	}

	void SetFixed(bool fixed) {
		this->fixed = fixed;
	}

	void Draw() {
		if (!bitmap) {
			return;
		}

		const Sprite* sprite = Main_Data::game_screen->GetPicture(pic_id)->GetSprite();
		if (!sprite) {
			return;
		}

		if (fixed) {
			DisplayUi->GetDisplaySurface()->Blit(x - Game_Map::GetDisplayX(), y - Game_Map::GetDisplayY(), *bitmap, bitmap->GetRect(), sprite->GetOpacity());
		} else {
			DisplayUi->GetDisplaySurface()->Blit(x, y, *bitmap, bitmap->GetRect(), sprite->GetOpacity());
		}
	};

	int GetZ() const {
		return z;
	}

	DrawableType GetType() const {
		return TypeDefault;
	}

	void Update() {
		const Sprite* sprite = Main_Data::game_screen->GetPicture(pic_id)->GetSprite();

		if (sprite) {
			if (z != sprite->GetZ()) {
				z = sprite->GetZ();
				Graphics::UpdateZCallback();
			}
		}
	}

	static int ParseParameter(bool& is_valid, std::u32string::iterator& text_index, std::u32string::iterator& end) {
		++text_index;

		if (text_index == end ||
			*text_index != '[') {
			--text_index;
			is_valid = false;
			return 0;
		}

		++text_index; // Skip the [

		bool null_at_start = false;
		std::stringstream ss;
		for (;;) {
			if (text_index == end) {
				break;
			} else if (*text_index == '\n') {
				--text_index;
				break;
			}
			else if (*text_index == '0') {
				// Truncate 0 at the start
				if (!ss.str().empty()) {
					ss << "0";
				} else {
					null_at_start = true;
				}
			}
			else if (*text_index >= '1' &&
					 *text_index <= '9') {
				ss << std::string(text_index, std::next(text_index));
			} else if (*text_index == ']') {
				break;
			} else {
				// End of number
				// Search for ] or line break
				while (text_index != end) {
					if (*text_index == '\n') {
						--text_index;
						break;
					} else if (*text_index == ']') {
						break;
					}
					++text_index;
				}
				break;
			}
			++text_index;
		}

		if (ss.str().empty()) {
			if (null_at_start) {
				ss << "0";
			} else {
				is_valid = false;
				return 0;
			}
		}

		int num;
		ss >> num;
		is_valid = true;
		return num;
	}

	static std::string ParseCommandCode(bool& success, std::u32string::iterator& text_index, std::u32string::iterator& end) {
		int parameter;
		bool is_valid;
		uint32_t cmd_char = *text_index;
		success = true;

		switch (cmd_char) {
			case 'n':
			case 'N':
				// Output Hero name
				parameter = ParseParameter(is_valid, text_index, end);
				if (is_valid) {
					Game_Actor* actor = NULL;
					if (parameter == 0) {
						// Party hero
						actor = Main_Data::game_party->GetActors()[0];
					} else {
						actor = Game_Actors::GetActor(parameter);
					}
					if (actor != NULL) {
						return actor->GetName();
					}
				}
				break;
			case 'v':
			case 'V':
				// Show Variable value
				parameter = ParseParameter(is_valid, text_index, end);
				if (is_valid && Game_Variables.IsValid(parameter)) {
					std::stringstream ss;
					ss << Game_Variables[parameter];
					return ss.str();
				} else {
					// Invalid Var is always 0
					return "0";
				}
			case 'i':
				parameter = ParseParameter(is_valid, text_index, end);
				if (is_valid && parameter > 0 && parameter <= Data::items.size()) {
					return Data::items[parameter - 1].name;
				}
				return "";
			case 'I':
				parameter = ParseParameter(is_valid, text_index, end);
				if (is_valid && parameter > 0 && parameter <= Data::items.size()) {
					return Data::items[parameter - 1].description;
				}
				return "";
			case 't':
				parameter = ParseParameter(is_valid, text_index, end);
				if (is_valid && parameter > 0 && parameter <= Data::skills.size()) {
					return Data::skills[parameter - 1].name;
				}
				return "";
			case 'T':
				parameter = ParseParameter(is_valid, text_index, end);
				if (is_valid && parameter > 0 && parameter <= Data::skills.size()) {
					return Data::skills[parameter - 1].description;
				}
				return "";
			case 'x':
			case 'X':
				// Take text of ID referenced by X (if exists) TODO
				{

				}
				return "";
			default:;
				// When this happens text_index was not on a \ during calling
		}
		success = false;
		return "";
	}

	static std::string Substitute(const std::string& text) {
		std::u32string::iterator text_index, end;
		std::u32string utext;

		utext = Utils::DecodeUTF32(text);
		text_index = utext.end();
		end = utext.end();

		uint32_t escape_char = Utils::DecodeUTF32(Player::escape_symbol).front();

		if (!utext.empty()) {
			// Move on first valid char
			--text_index;

			// Apply commands that insert text
			while (std::distance(text_index, utext.begin()) <= -1) {
				switch (tolower(*text_index--)) {
					case 'n':
					case 'v':
					case 'i':
					case 't':
					case 'x':
					{
						if (*text_index != escape_char) {
							continue;
						}
						++text_index;

						auto start_code = text_index - 1;
						bool success;
						std::u32string command_result = Utils::DecodeUTF32(ParseCommandCode(success, text_index, end));
						if (!success) {
							text_index = start_code - 2;
							continue;
						}
						utext.replace(start_code, text_index + 1, command_result);
						// Start from the beginning, the inserted text might add new commands
						text_index = utext.end();

						// Move on first valid char
						--text_index;

						break;
					}
					default:
						break;
				}
			}
		}

		return Utils::EncodeUTF(utext);
	}

private:
	void Refresh() {
		if (texts.empty()) {
			bitmap.reset();
			return;
		}

		int width = 0;
		int height = 0;

		const FontRef& font = Font::Default();

		for (auto& t : texts) {
			t = Substitute(t);

			Rect r = font->GetSize(t);
			width = std::max(width, r.width);
			height += r.height + 2;
		}

		bitmap = Bitmap::Create(width, height, true);

		height = 0;
		for (auto& t : texts) {
			bitmap->TextDraw(0, height, color, t);
			height += font->GetSize(t).height + 2;
		}
	}

	std::vector<std::string> texts;
	BitmapRef bitmap;
	int x = 0;
	int y = 0;
	int z = 0;
	int pic_id = 1;
	int color = 0;
	bool fixed = false;
};

DynRpgText* get_text(const std::string& id, bool silent = false) {
	std::string new_id = DynRpgText::Substitute(id);

	auto it = graphics.find(new_id);
	if (it == graphics.end()) {
		if (!silent) {
			Output::Warning("No text with ID %s found", new_id.c_str());
		}
		return nullptr;
	}

	return (*it).second.get();
}

static bool WriteText(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("write_text")

	DYNRPG_CHECK_ARG_LENGTH(4);

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_INT_ARG(1, x)
	DYNRPG_GET_INT_ARG(2, y)
	DYNRPG_GET_STR_ARG(3, text)

	const std::string new_id = DynRpgText::Substitute(id);
	graphics[new_id] = std::unique_ptr<DynRpgText>(new DynRpgText(1, x, y + 2, text));

	if (args.size() > 4) {
		DYNRPG_GET_STR_ARG(4, fixed)
		graphics[new_id]->SetFixed(fixed == "fixed");
	}

	if (args.size() > 5) {
		DYNRPG_GET_INT_ARG(5, color)
		graphics[new_id]->SetColor(color);
	}

	if (args.size() > 6) {
		DYNRPG_GET_INT_ARG(6, pic_id)
		graphics[new_id]->SetPictureId(pic_id);
	}

	return true;
}

static bool AppendLine(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("append_line")

	DYNRPG_CHECK_ARG_LENGTH(2);

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_STR_ARG(1, text)

	DynRpgText* handle = get_text(id);

	if (!handle) {
		return true;
	}

	handle->AddLine(text);

	return true;
}

static bool AppendText(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("append_text")

	DYNRPG_CHECK_ARG_LENGTH(2);

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_STR_ARG(1, text)

	DynRpgText* handle = get_text(id);

	if (!handle) {
		return true;
	}

	handle->AddText(text);

	return true;
}

static bool ChangeText(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("change_text")

	DYNRPG_CHECK_ARG_LENGTH(3);

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_STR_ARG(1, text)
	DYNRPG_GET_INT_ARG(2, color)

	DynRpgText* handle = get_text(id);

	if (!handle) {
		return true;
	}

	handle->ClearText();
	handle->SetColor(color);
	handle->AddText(text);

	return true;
}

static bool ChangePosition(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("change_position")

	DYNRPG_CHECK_ARG_LENGTH(3);

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_INT_ARG(1, x)
	DYNRPG_GET_INT_ARG(2, y)

	DynRpgText* handle = get_text(id);

	if (!handle) {
		return true;
	}

	// Offset is somehow wrong compared to RPG_RT
	handle->SetPosition(x, y + 2);

	return true;
}

static bool RemoveText(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("remove_text")

	DYNRPG_CHECK_ARG_LENGTH(2);

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_STR_ARG(1, nothing)

	DynRpgText* handle = get_text(id);

	if (!handle) {
		return true;
	}

	handle->ClearText();

	return true;
}

static bool RemoveAll(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("remove_all")

	DYNRPG_CHECK_ARG_LENGTH(0);

	graphics.clear();

	return true;
}

void DynRpg::TextPlugin::RegisterAll() {
	DynRpg::RegisterFunction("write_text", WriteText);
	DynRpg::RegisterFunction("append_line", AppendLine);
	DynRpg::RegisterFunction("append_text", AppendText);
	DynRpg::RegisterFunction("change_text", ChangeText);
	DynRpg::RegisterFunction("change_position", ChangePosition);
	DynRpg::RegisterFunction("remove_text", RemoveText);
	DynRpg::RegisterFunction("remove_all", RemoveAll);
}

void DynRpg::TextPlugin::Update() {
	for (auto& g : graphics) {
		g.second->Update();
	}
}
