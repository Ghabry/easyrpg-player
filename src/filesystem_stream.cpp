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

#include "filesystem_stream.h"

Filesystem_Stream::InputStream::InputStream(std::streambuf* sb) : std::istream(sb) {}

Filesystem_Stream::InputStream::~InputStream() {
	delete rdbuf();
}

Filesystem_Stream::InputStream::InputStream(InputStream&& is) noexcept : std::istream(std::move(is)) {
	set_rdbuf(is.rdbuf());
	is.set_rdbuf(nullptr);
}

Filesystem_Stream::InputStream& Filesystem_Stream::InputStream::operator=(InputStream&& is) noexcept {
	if (this == &is) return is;
	std::istream::operator=(std::move(is));
	set_rdbuf(is.rdbuf());
	is.set_rdbuf(nullptr);
	return is;
}

Filesystem_Stream::OutputStream::OutputStream(std::streambuf* sb, FilesystemView fs) :
	std::ostream(sb), fs(fs) {};

Filesystem_Stream::OutputStream::~OutputStream() {
	delete rdbuf();
	if (fs) {
		fs.ClearCache();
	}
}

Filesystem_Stream::OutputStream::OutputStream(OutputStream&& os) noexcept : std::ostream(std::move(os)) {
	set_rdbuf(os.rdbuf());
	os.set_rdbuf(nullptr);
}

Filesystem_Stream::OutputStream& Filesystem_Stream::OutputStream::operator=(OutputStream&& os) noexcept {
	if (this == &os) return os;
	std::ostream::operator=(std::move(os));
	set_rdbuf(os.rdbuf());
	os.set_rdbuf(nullptr);
	return os;
}

Filesystem_Stream::InputStreamBuf::InputStreamBuf() : std::streambuf() {
	buffer.resize(4096);
}

void Filesystem_Stream::InputStreamBuf::Invalidate() {
	char* char_buffer = reinterpret_cast<char*>(buffer.data());
	setg(char_buffer, char_buffer + buffer.size(), char_buffer + buffer.size());
}

void Filesystem_Stream::InputStreamBuf::Filled(size_t bytes) {
	char* char_buffer = reinterpret_cast<char*>(buffer.data());
	setg(char_buffer, char_buffer, char_buffer + bytes);
}

std::streambuf::int_type Filesystem_Stream::InputStreamBuf::underflow() {
	auto bytes_read = FillBuffer(buffer.data(), buffer.size());
	char* char_buffer = reinterpret_cast<char*>(buffer.data());
	assert(bytes_read <= buffer.size());
	if (bytes_read == 0) {
		return traits_type::eof();
	} else if (bytes_read > 0) {
		Filled(bytes_read);
	}
	else {
		Invalidate();
	}
	return bytes_read;
}

std::streambuf::pos_type Filesystem_Stream::InputStreamBuf::seekoff(std::streambuf::off_type offset, std::ios_base::seekdir dir, std::ios_base::openmode) {
	if (dir == std::ios_base::cur) {
		int after = gptr() - egptr() + offset;
		int before = gptr() - eback() + offset;

		if (after > 0) {
			Invalidate();
			cur_pos = Seek(after, dir);
		}
		else if (before > 0) {
			Invalidate();
			cur_pos = Seek(-before, dir);
		} else {
			gbump(offset);
			cur_pos += offset;
		}
	} else if (dir == std::ios_base::end) {
		// FIXME: Seek end can be buffered
		Invalidate();
		cur_pos = Seek(offset, dir);
	} else if (dir == std::ios_base::beg) {
		// FIXME: Seek beg can be buffered
		Invalidate();
		cur_pos = Seek(offset, dir);
	}

	return cur_pos;
}

std::streambuf::pos_type Filesystem_Stream::InputStreamBuf::seekpos(std::streambuf::pos_type pos, std::ios_base::openmode mode) {
	return seekoff(pos, std::ios_base::beg, mode);
}
