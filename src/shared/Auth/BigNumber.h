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

#ifndef _AUTH_BIGNUMBER_H
#define _AUTH_BIGNUMBER_H

#include "Common.h"

DISABLE_WARNING_PUSH
#include <cryptopp/integer.h>
DISABLE_WARNING_POP

class BigNumber
{
public:
    BigNumber() : m_array(nullptr) {}
    BigNumber(uint32 intVal) : m_integer(intVal), m_array(nullptr) {}
    BigNumber(CryptoPP::Integer intVal) : m_integer(intVal), m_array(nullptr) {}

    // modifiers

    void SetBinary(const uint8* bytes, int len);
    int SetHexStr(const char* str);
    void SetRand(int numbits);

    // operators

    BigNumber operator=(const BigNumber& bn);

    BigNumber operator+=(const BigNumber& bn);
    BigNumber operator+(const BigNumber& bn)
    {
        BigNumber t(*this);
        return t += bn;
    }
    BigNumber operator-=(const BigNumber& bn);
    BigNumber operator-(const BigNumber& bn)
    {
        BigNumber t(*this);
        return t -= bn;
    }
    BigNumber operator*=(const BigNumber& bn);
    BigNumber operator*(const BigNumber& bn)
    {
        BigNumber t(*this);
        return t *= bn;
    }
    BigNumber operator/=(const BigNumber& bn);
    BigNumber operator/(const BigNumber& bn)
    {
        BigNumber t(*this);
        return t /= bn;
    }
    BigNumber operator%=(const BigNumber& bn);
    BigNumber operator%(const BigNumber& bn)
    {
        BigNumber t(*this);
        return t %= bn;
    }

    BigNumber ModExp(const BigNumber& bn1, const BigNumber& bn2);
    BigNumber Exp(const BigNumber&);

    // getters
    bool isZero() const;
    int GetNumBytes(void) const;
    uint32 AsDword() const;
    uint8* AsByteArray(int minSize = 0);
    std::string AsHexStr() const;

private:
    uint8* m_array;
    CryptoPP::Integer m_integer;
};

#endif

