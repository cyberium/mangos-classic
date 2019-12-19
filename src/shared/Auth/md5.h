/*
 * This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
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

#ifndef _AUTH_MD5_H
#define _AUTH_MD5_H

#include "Common.h"

#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include "cryptopp/md5.h"

#define MD5_DIGEST_LENGTH CryptoPP::Weak::MD5::DIGESTSIZE

/*
class Md5Hash
{
public:
    static int GetLength(void) { return CryptoPP::Weak::MD5::DIGESTSIZE; };
};*/
#endif
