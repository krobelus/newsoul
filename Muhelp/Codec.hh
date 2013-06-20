/* Muhelp - Helper library for Museek
 *
 * Copyright (C) 2003-2004 Hyriand <hyriand@thegraveyard.org>
 * Copyright 2008 little blue poney <lbponey@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __CODEC_HH__
#define __CODEC_HH__

#include <string>
#include <iconv.h>
#include <errno.h>

#ifndef ICONV_CONST
# define ICONV_CONST
#endif /* ! ICONV_CONST */
#define ICONV_IN ICONV_CONST char **

class Codec {
public:
	Codec(const std::string& from, const std::string& to);
	~Codec();
	
	inline bool valid() const { return mCD != (iconv_t)-1; }
	
	std::string convert(const std::string& text);
	std::wstring wide(const std::string& text);
	std::string narrow(const std::wstring& to);
	
	static std::string convert(const std::string& text, const std::string& from, const std::string& to);
	static std::wstring wide(const std::string& text, const std::string& from);
	static std::string narrow(const std::wstring& text, const std::string& to);
private:
	iconv_t mCD;
};

#endif // __CODEC_HH__