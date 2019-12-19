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

#ifndef _AUTH_SHA1_H
#define _AUTH_SHA1_H

#include "Common.h"

DISABLE_WARNING_PUSH
#include "cryptopp/sha.h"
DISABLE_WARNING_POP

#define SHA_DIGEST_LENGTH CryptoPP::SHA1::DIGESTSIZE

class BigNumber;

typedef std::unique_ptr<uint8[]> SHA1DigestUPtr;
class Sha1Hash
{
    public:
        void Initialize();
        void Finalize();

        void UpdateBigNumbers(BigNumber* bn0, ...);

        void UpdateData(const uint8* dta, int len);
        void UpdateData(const std::string& str);

        uint8 const* GetDigest(void) const;
        std::string AsHexStr() const;
        static int GetLength(void) { return CryptoPP::SHA1::DIGESTSIZE; };

    private:
        CryptoPP::SHA1 m_sha1;
        uint8 m_digest[CryptoPP::SHA1::DIGESTSIZE];
};
#endif
