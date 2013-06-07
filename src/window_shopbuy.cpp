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
#include <sstream>
#include <string>
#include "window_base.h"
#include "window_shopbuy.h"
#include "game_system.h"
#include "game_temp.h"
#include "game_party.h"
#include "font.h"
#include "bitmap.h"
#include "data.h"
#include "window_help.h"

Window_ShopBuy::Window_ShopBuy(int ix, int iy, int iwidth, int iheight) :
	Window_Selectable(ix, iy, iwidth, iheight) {
	index = 0;
}

int Window_ShopBuy::GetItemId() {
	if (index < 0) {
		return 0;
	} else {
		return data[index];
	}
}

void Window_ShopBuy::Refresh() {
	data = Game_Temp::shop_goods;
	item_max = data.size();

	CreateContents();

	contents->clear();
	Rect rect(0, 0, contents->width(), contents->height());
	contents->clear();

	for (size_t i = 0; i < data.size(); ++i) {
		DrawItem(i);
	}
}

void Window_ShopBuy::DrawItem(int index) {
	int item_id = data[index];
	bool enabled = Data::items[item_id - 1].price <= Game_Party::GetGold();
	Rect rect = GetItemRect(index);
	contents->fill(rect, Color());
	DrawItemName(&Data::items[item_id - 1], rect.x, rect.y, enabled);

	std::ostringstream ss;
	ss << Data::items[item_id - 1].price;
	contents->draw_text(rect.width + 4, rect.y, ss.str(), enabled ? Font::ColorDefault : Font::ColorDisabled, Text::AlignRight);
}

void Window_ShopBuy::UpdateHelp() {
	help_window->SetText(GetItemId() == 0 ? "" :
		Data::items[GetItemId() - 1].description);
}

bool Window_ShopBuy::CheckEnable(int item_id) {
	return (
		item_id > 0 &&
		Data::items[item_id - 1].price <= Game_Party::GetGold() &&
		Game_Party::ItemNumber(item_id) < 99);
}
