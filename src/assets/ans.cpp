// Copyright (c) 2022 The Avian Core developers
// Copyright (c) 2022 Shafil Alam
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ans.h"

#include <iostream>
#include <string>
#include <sstream>

#include "string.h"

#include "univalue.h"
#include "util.h"
#include "utilstrencodings.h"
#include "base58.h"
#include "script/standard.h"

#include "boost/asio.hpp"

using namespace boost::asio;

// TODO: Clean this up.
// TODO: IP 1.1.1.1 is encoded incorrect.
static std::string IPToHex(const char* strip)
{
   unsigned int ip = 0;
   char* str = strdup(strip);
   char delimiters[] = ".";
   char* token = strtok(str, delimiters);
   int tokencount = 0;
   while(token != NULL)
   {
      if(tokencount < 4)
      {
         int n = atoi(token);
         ip = ip | n << 8 * (3 - tokencount);
      }
      tokencount++;
      token = strtok(NULL, delimiters);
   }
   free(str);

   if(tokencount != 4)
      ip = 0;

    std::stringstream ss;
    ss << std::hex << ip;

   return ss.str();
}

// TODO: Clean this up.
// TODO: IP 1.1.1.1 is encoded incorrect.
// If invalid hex, return 0.
static std::string HexToIP(const char *input)
{
    char *output = (char*)malloc(sizeof(char) * 16);
    unsigned int a, b, c, d;

    if (sscanf(input, "%2x%2x%2x%2x", &a, &b, &c, &d) != 4)
        return output;
    sprintf(output, "%u.%u.%u.%u", a, b, c, d);
    return std::string(output);
}

bool CAvianNameSystemID::CheckIP(std::string rawip, bool isHex) {
    std::string ip = rawip;
    if (isHex) ip = HexToIP(rawip.c_str());

    boost::system::error_code error;
    ip::address_v4::from_string(ip, error);

    if (error) return false;
    else return true;
}

// TODO: Add error result?
bool CAvianNameSystemID::CheckTypeData(Type type, std::string typeData) {
    if (type == Type::ADDR) {
        CTxDestination destination = DecodeDestination(typeData);
        if (!IsValidDestination(destination)) return false;
    } else if (type == Type::IP) {
        if (!CheckIP(typeData, true)) return false;
    } else {
        // Unknown type
        return false;
    }
    return true;
}

std::string CAvianNameSystemID::FormatTypeData(Type type, std::string typeData, std::string& error)
{
    std::string returnStr = typeData;

    // Check and set type data
    if (type == ADDR) {
        CTxDestination destination = DecodeDestination(typeData);
        if (!IsValidDestination(destination)) {
            error = (typeData != "") 
            ? std::string("Invalid Avian address: ") + typeData 
            : std::string("Empty Avian address.");
        }
    } else if (type == IP) {
        if (!CheckIP(typeData, false)) {
            error = (typeData != "") 
            ? std::string("Invalid IPv4 address: ") + typeData 
            : std::string("Empty IPv4 addresss.");
        }
        returnStr = IPToHex(typeData.c_str());
    }

    return returnStr;
}

bool CAvianNameSystemID::IsValidID(std::string ansID) {
    // Check for min length
    if(ansID.length() <= prefix.size() + 1) return false;

    // Check for prefix
    bool hasPrefix = (ansID.substr(0, CAvianNameSystemID::prefix.length()) == CAvianNameSystemID::prefix) && (ansID.size() <= 64);
    if (!hasPrefix) return false;

    // Must be valid hex char
    std::string hexStr = ansID.substr(prefix.length(), 1);
    if (!IsHexNumber(hexStr)) return false;

    // Hex value must be less than 0xf
    int hexInt = stoi(hexStr, 0, 16);
    if (hexInt > 0xf) return false;

    // Check type
    Type type = static_cast<Type>(hexInt);
    std::string rawData = ansID.substr(prefix.length() + 1);

    if (!CheckTypeData(type, rawData)) return false;

    return true;
}

CAvianNameSystemID::CAvianNameSystemID(Type type, std::string rawData) :
    m_addr(""),
    m_ip("")
{
    this->m_type = type;

    if (!CheckTypeData(this->m_type, rawData)) return;

    if (this->m_type == Type::ADDR) {
        // Avian address
        this->m_addr = rawData;
    }
    else if (this->m_type == Type::IP) {
        // Raw IP (127.0.0.1)
        this->m_ip = HexToIP(rawData.c_str());
    }
}

CAvianNameSystemID::CAvianNameSystemID(std::string ansID) :
    m_addr(""),
    m_ip("")
{
    // Check if valid ID
    if(!IsValidID(ansID)) return;

    // Get type
    Type type = static_cast<Type>(stoi(ansID.substr(prefix.length(), 1), 0, 16));
    this->m_type = type;

    // Set info based on data
    std::string data = ansID.substr(prefix.length() + 1); // prefix + type

    if (this->m_type == Type::ADDR) {
        this->m_addr = data;
    } else if(this->m_type == Type::IP) {
        this->m_ip = HexToIP(data.c_str());
    }
}

std::string CAvianNameSystemID::to_string() {
    std::string id = "";

    // 1. Add prefix
    id += prefix;

    // 2. Add type
    std::stringstream ss;
    ss << std::hex << this->m_type;
    id += ss.str();

    // 3. Add data
    if (this->m_type == Type::ADDR) {
       id += m_addr;
    } else if (this->m_type == Type::IP) {
        id += IPToHex(m_ip.c_str());
    }

    return id;
}

UniValue CAvianNameSystemID::to_object()
{
    UniValue ansInfo(UniValue::VOBJ);

    ansInfo.pushKV("ans_id", this->to_string());
    ansInfo.pushKV("type_hex", this->type());

    if (this->type() == CAvianNameSystemID::ADDR) {
        ansInfo.pushKV("ans_addr", this->addr());
    } else if (this->type() == CAvianNameSystemID::IP) {
        ansInfo.pushKV("ans_ip", this->ip());
    }

    return ansInfo;
}
