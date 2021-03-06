/*  newsoul - A SoulSeek client written in C++
    Copyright (C) 2006-2007 Ingmar K. Steen (iksteen@gmail.com)
    Copyright 2008 little blue poney <lbponey@users.sourceforge.net>
    Karol 'Kenji Takahashi' Woźniak © 2013 - 2014

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 */

#include "codesetmanager.h"

newsoul::CodesetManager::CodesetManager(Newsoul * newsoul) : m_Newsoul(newsoul)
{
}

newsoul::CodesetManager::~CodesetManager()
{
  /* Free all the stored iconv contexts. */
  std::map<std::pair<std::string, std::string>, iconv_t>::iterator it, end = m_Contexts.end();
  for(it = m_Contexts.begin(); it != end; ++it)
    iconv_close((*it).second);
}

std::string
newsoul::CodesetManager::convert(const std::string & from, const std::string & to, const std::string & str)
{
  /* No point in trying to convert an empty string. */
  if(str.empty())
    return str;

  /* Get our iconv conversion context. */
  iconv_t context = getContext(from, to);
  /* Guess and allocate a buffer. str.size() * 4 should be enough to hold a
     converted string to any character set, even UTF32. */
  size_t buf_len = str.size() * 4 + 1;
  char * out_buf = new char[buf_len];

  /* Fetch pointer to the source data. */
  size_t r, in_left, out_left;
  in_left = str.size();
#ifndef _ICONV_H_
  char *inbuf = const_cast<char*>(str.data());
#else
  const char *inbuf = str.data();
#endif
  char * out_ptr = out_buf;
  out_left = buf_len;

  /* Create the result string. */
  std::string ret = std::string();

  while(1)
  {
    /* Try to convert the string. */
    r = iconv(context, &inbuf, &in_left, &out_ptr, &out_left);
    if(r == (size_t)-1) {
        if (errno == E2BIG) { // Output buffer not large enough
            delete [] out_buf;
            buf_len *= 2; // Try a buffer twice as large
            out_buf = new char[buf_len];
            in_left = str.size();
            out_ptr = out_buf;
            out_left = buf_len;
            continue;
        }
        else if (errno == EILSEQ) { // Invalid input char
            in_left--; // Move to the next char
            inbuf++; // move the input buffer
            ret.append(out_buf, buf_len - out_left);
            ret.append("�");
            // reset out buffer
            delete [] out_buf;
            out_buf = new char[buf_len];
            out_ptr = out_buf;
            out_left = buf_len;
            continue;
        }
    }

    break;
  }

  if(r != (size_t)-1 && in_left == 0)
    ret.append(out_buf, buf_len - out_left);

  // Free the output buffer.
  delete [] out_buf;
  return ret;
}

std::string
newsoul::CodesetManager::getNetworkCodeset(std::initializer_list<const std::string> key) const
{
  // Try to get the requested character set
  std::string codeset = newsoul()->config()->getStr(key);
  if(codeset.empty()) // Get the default network encoding
    codeset = newsoul()->config()->getStr({"encoding", "network"});
  if(codeset.empty()) // Fall back to UTF-8
    codeset = "UTF-8";
  return codeset;
}

std::string
newsoul::CodesetManager::fromRoom(const std::string & room, const std::string & str)
{
  return convert(getNetworkCodeset({"encoding", "rooms", room}), "UTF-8", str);
}

std::map<std::string, std::string>
newsoul::CodesetManager::fromRoomMap(const std::string & room, const std::map<std::string, std::string> & map)
{
  // Get the character set for the room.
  std::string codeset = getNetworkCodeset({"encoding", "rooms", room});

  std::map<std::string, std::string> result;

  // Iterate over the map, convert values and store them in 'result'
  std::map<std::string, std::string>::const_iterator it, end = map.end();
  for(it = map.begin(); it != end; ++it)
    result[(*it).first] = convert(codeset, "UTF-8", (*it).second);

  return result;
}

std::string
newsoul::CodesetManager::toRoom(const std::string & room, const std::string & str)
{
    return convert("UTF-8", getNetworkCodeset({"encoding", "rooms", room}), str);
}

std::string
newsoul::CodesetManager::fromPeer(const std::string & peer, const std::string & str)
{
  return convert(getNetworkCodeset({"encoding", "users", peer}), "UTF-8", str);
}

std::string
newsoul::CodesetManager::toPeer(const std::string & peer, const std::string & str)
{
    return convert("UTF-8", getNetworkCodeset({"encoding", "users", peer}), str);
}

std::string
newsoul::CodesetManager::fromFSToNet(const std::string & str, bool slashes)
{
    std::string strToConvert = str;
    if (slashes)
        strToConvert = string::replace(str, os::separator(), '\\');
    return convert(getNetworkCodeset({"encoding", "local"}), getNetworkCodeset({"encoding", "network"}), strToConvert);
}

std::string
newsoul::CodesetManager::fromNetToFS(const std::string & str, bool slashes)
{
    std::string strToConvert = str;
    if (slashes)
        strToConvert = string::replace(str, '\\', os::separator());
    return convert(getNetworkCodeset({"encoding", "network"}), getNetworkCodeset({"encoding", "local"}), strToConvert);
}

std::string
newsoul::CodesetManager::fromFSToPeer(const std::string & peer, const std::string & str, bool slashes)
{
    std::string strToConvert = str;
    if (slashes)
        strToConvert = string::replace(str, os::separator(), '\\');
    return convert(getNetworkCodeset({"encoding", "local"}), getNetworkCodeset({"encoding", "users", peer}), strToConvert);
}

std::string
newsoul::CodesetManager::fromPeerToFS(const std::string & peer, const std::string & str, bool slashes)
{
    std::string strToConvert = str;
    if (slashes)
        strToConvert = string::replace(str, '\\', os::separator());
    return convert(getNetworkCodeset({"encoding", "network"}), getNetworkCodeset({"encoding", "users", peer}), strToConvert);
}

std::string
newsoul::CodesetManager::fromUtf8ToFS(const std::string & str, bool slashes)
{
    std::string strToConvert = str;
    if (slashes)
        strToConvert = string::replace(str, '\\', os::separator());
    return fromUtf8(getNetworkCodeset({"encoding", "local"}), strToConvert);
}

std::string
newsoul::CodesetManager::fromFsToUtf8(const std::string & str, bool slashes)
{
    std::string strToConvert = str;
    if (slashes)
        strToConvert = string::replace(str, os::separator(), '\\');
    return toUtf8(getNetworkCodeset({"encoding", "local"}), strToConvert);
}

std::string
newsoul::CodesetManager::fromNet(const std::string & str)
{
    return convert(getNetworkCodeset({"encoding", "network"}), "UTF-8", str);
}

std::string
newsoul::CodesetManager::toNet(const std::string & str)
{
    return convert("UTF-8", getNetworkCodeset({"encoding", "network"}), str);
}

std::string
newsoul::CodesetManager::fromUtf8ToNet(const std::string & str)
{
    return fromUtf8(getNetworkCodeset({"encoding", "network"}), str);
}

std::string
newsoul::CodesetManager::fromNetToUtf8(const std::string & str)
{
    return toUtf8(getNetworkCodeset({"encoding", "network"}), str);
}

iconv_t
newsoul::CodesetManager::getContext(const std::string & from, const std::string & to)
{
  // This is the context store key.
  std::pair<std::string, std::string> key(from, to);
  // Check if we've already constructed this iconv context.
  std::map<std::pair<std::string, std::string>, iconv_t>::iterator it;
  it = m_Contexts.find(key);
  if(it != m_Contexts.end())
    return (*it).second;

  // Create the iconv context for this conversion.
  iconv_t context = iconv_open(to.c_str(), from.c_str());
  // Currently, we don't handle invalid contexts very well.
  assert(context != (iconv_t)-1);
  // Store the context for future use.
  m_Contexts[key] = context;
  return context;
}
