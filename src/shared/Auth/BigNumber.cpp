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

#include "Auth/BigNumber.h"
#include <cryptopp/osrng.h>
#include <cryptopp/algebra.h>
#include <cryptopp/hex.h>
#include "Log.h"

using namespace CryptoPP;

void BigNumber::SetBinary(const uint8* bytes, int len)
{
    for (int i = 0; i < len; ++i)
        m_integer.SetByte(i, bytes[i]);
}

int BigNumber::SetHexStr(const char* str)
{
    //TODO:: Do we have to take care about endianness here?
    std::string hexStr(str);
    hexStr += "h";
    m_integer = Integer(hexStr.c_str());

    return 1;
}

void BigNumber::SetRand(int numbits)
{
    AutoSeededRandomPool rGen;
    m_integer = Integer(rGen, numbits);
}

BigNumber BigNumber::operator+=(const BigNumber& bn)
{
    m_integer += bn.m_integer;
    return *this;
}

BigNumber BigNumber::operator-=(const BigNumber& bn)
{
    m_integer -= bn.m_integer;
    return *this;
}

BigNumber BigNumber::operator*=(const BigNumber& bn)
{
    m_integer *= bn.m_integer;
    return *this;
}

BigNumber BigNumber::operator/=(const BigNumber& bn)
{
    m_integer /= bn.m_integer;
    return *this;
}

BigNumber BigNumber::operator%=(const BigNumber& bn)
{
    m_integer %= bn.m_integer;
    return *this;
}

BigNumber BigNumber::operator=(const BigNumber& bn)
{
    m_integer = bn.m_integer;
    return *this;
}

BigNumber BigNumber::ModExp(const BigNumber& bn1, const BigNumber& bn2)
{
    //r = a ^ p % m
    return BigNumber(a_exp_b_mod_c(m_integer, bn1.m_integer, bn2.m_integer));
}

BigNumber BigNumber::Exp(const BigNumber& bn)
{
    return BigNumber((EuclideanDomainOf<Integer>().Exponentiate(m_integer, bn.m_integer)));
}

bool BigNumber::isZero() const
{
    return m_integer.IsZero();
}

int BigNumber::GetNumBytes(void) const
{
    return static_cast<int>(m_integer.ByteCount());
}

uint32 BigNumber::AsDword() const
{
    return static_cast<int>(m_integer.ConvertToLong());
}

uint8* BigNumber::AsByteArray(int minSize /*= 0*/)
{
    int length = (minSize >= GetNumBytes()) ? minSize : GetNumBytes();
    delete[] m_array;
    m_array = new uint8[length];

    // If we need more bytes than length of BigNumber set the rest to 0
    if (length > GetNumBytes())
        memset((void*)m_array, 0, length);

    m_integer.Encode(m_array, length);

    std::reverse(m_array, m_array + length);
    return m_array;
}

std::string BigNumber::AsHexStr() const
{
    const unsigned int UPPER = (1 << 31);
    return IntToString(m_integer, (UPPER | 16));
}
