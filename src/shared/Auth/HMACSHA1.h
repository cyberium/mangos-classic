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

#ifndef _AUTH_HMACSHA1_H
#define _AUTH_HMACSHA1_H

#include "Common.h"3
#include <Auth/Sha1.h>
#include <cryptopp/hmac.h>

class BigNumber;

#define SEED_KEY_SIZE 16

class HMACSHA1
{
    public:
        HMACSHA1(uint32 len, uint8* seed);
        HMACSHA1(uint32 len, uint8* seed, bool);
        ~HMACSHA1();
        void UpdateBigNumber(BigNumber* bn);
        void UpdateData(const uint8* data, int length);
        void Initialize();
        void Finalize();
        uint8* GetDigest() { return m_digest; };
        static int GetLength() { return CryptoPP::SHA1::DIGESTSIZE; };
    private:
        std::unique_ptr<CryptoPP::HMAC<CryptoPP::SHA1>> m_hmac;
        uint8 m_key[SEED_KEY_SIZE];
        uint8 m_digest[CryptoPP::SHA1::DIGESTSIZE];
};
#endif
