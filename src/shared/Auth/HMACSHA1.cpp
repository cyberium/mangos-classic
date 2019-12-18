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

#include "Auth/HMACSHA1.h"
#include "BigNumber.h"

using namespace CryptoPP;

HMACSHA1::HMACSHA1(uint32 len, uint8* seed) : m_hmac(new HMAC< SHA1 >(seed, len))
{
    memcpy(&m_key, seed, len);
}

HMACSHA1::HMACSHA1(uint32 len, uint8* seed, bool) : m_hmac(new HMAC< SHA1 >(seed, len)) // to get over the default constructor
{
}

HMACSHA1::~HMACSHA1()
{
    memset(&m_key, 0x00, SEED_KEY_SIZE);
}

void HMACSHA1::UpdateBigNumber(BigNumber* bn)
{
    UpdateData(bn->AsByteArray(), bn->GetNumBytes());
}

void HMACSHA1::UpdateData(const uint8* data, int length)
{
    m_hmac->Update(data, length);
}

void HMACSHA1::Initialize()
{
    m_hmac->Restart();
}

void HMACSHA1::Finalize()
{
    m_hmac->Final(m_digest);
}
