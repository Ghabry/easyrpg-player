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

void Filesystem_Stream::InputStreamBuf::EmptyBuffer() {
	setg(eback(), egptr(), egptr());
}

void Filesystem_Stream::InputStreamBuf::Filled(size_t bytes) {

}

std::size_t Filesystem_Stream::InputStreamBuf::Size() {
	return egptr() - eback();
}

std::size_t Filesystem_Stream::InputStreamBuf::Remaining() {
	return egptr() - gptr();
}

std::streambuf::pos_type Filesystem_Stream::InputStreamBuf::seekoff(std::streambuf::off_type offset, std::ios_base::seekdir dir, std::ios_base::openmode mode) {
	bool empty = (gptr() == egptr());

	if (dir == std::ios_base::cur) {
		gbump(offset);
	} else if (dir == std::ios_base::end) {
		setg(eback(), egptr() + offset, egptr());
	} else if (dir == std::ios_base::beg) {
		setg(eback(), eback() + offset, egptr());
	}

	auto ret = gptr() - eback();
	if (empty) {
		EmptyBuffer();
	}

	return ret;
}
