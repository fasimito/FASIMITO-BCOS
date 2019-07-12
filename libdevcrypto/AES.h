#pragma once
#include "Common.h"
namespace dev
{
    DEV_SIMPLE_EXCEPTION(AESKeyLengthError);
    bytes aesCBCEncrypt(bytesConstRef _plainData, bytesConstRef _key);
    bytes aesCBCDecrypt(bytesConstRef _cypherData, bytesConstRef _key);
    bytes readableKeyBytes(const std::string& _readableKey);
}  // namespace dev
