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

#include "Auth/Sha1.h"
#include "Auth/BigNumber.h"

#include <cstdarg>

using namespace CryptoPP;

void Sha1Hash::Initialize()
{
    m_sha1.Restart();
}

void Sha1Hash::Finalize()
{
    m_sha1.Final(m_digest);
}

void Sha1Hash::UpdateBigNumbers(BigNumber* bn0, ...)
{
    va_list v;

    va_start(v, bn0);
    BigNumber* bn = bn0;
    while (bn)
    {
        UpdateData(bn->AsByteArray(), bn->GetNumBytes());
        bn = va_arg(v, BigNumber*);
    }
    va_end(v);
}

void Sha1Hash::UpdateData(const uint8* dta, int len)
{
    m_sha1.Update(dta, len);
}

void Sha1Hash::UpdateData(const std::string& str)
{
    UpdateData((uint8 const*)str.c_str(), str.length());
}

uint8 const* Sha1Hash::GetDigest(void) const
{
    return m_digest;
}
