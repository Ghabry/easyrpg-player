# This file is part of EasyRPG Player.
#
# EasyRPG Player is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# EasyRPG Player is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.

# Window Face Class.
class Window_Face < Window_Base
	# Constructor.
  def initialize(x, y, w, h)
    super x, y, w, h
    self.contents = Bitmap.new w - 16, h - 16
  end

	# Renders the current face on the window.
  def refresh
    contents.clear
    draw_actor_face Game_Actors.actor(@actor_id), 0, 0
  end

  def set(id)
    @actor_id = id
    refresh
  end
end
