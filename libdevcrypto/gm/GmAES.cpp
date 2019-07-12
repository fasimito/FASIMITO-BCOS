#include "libdevcrypto/AES.h"
#include "libdevcrypto/Exceptions.h"
#include "sm4/sm4.h"
#include <libdevcore/easylog.h>
#include <openssl/sm4.h>
#include <stdlib.h>
#include <string.h>

using namespace dev;
using namespace dev::crypto;
using namespace std;


bytes dev::aesCBCEncrypt(bytesConstRef _plainData, bytesConstRef _key)
{
    bytesConstRef ivData = _key.cropped(0, 16);
    int padding = _plainData.size() % 16;
    int nSize = 16 - padding;
    int inDataVLen = _plainData.size() + nSize;
    bytes inDataV(inDataVLen);
    memcpy(inDataV.data(), (unsigned char*)_plainData.data(), _plainData.size());
    memset(inDataV.data() + _plainData.size(), nSize, nSize);

    bytes enData(inDataVLen);
    SM4::getInstance().setKey((unsigned char*)_key.data(), _key.size());
    SM4::getInstance().cbcEncrypt(
        inDataV.data(), enData.data(), inDataVLen, (unsigned char*)ivData.data(), 1);
    return enData;
}
bytes dev::aesCBCDecrypt(bytesConstRef _cypherData, bytesConstRef _key)
{
    bytesConstRef ivData = _key.cropped(0, 16);
    bytes deData(_cypherData.size());
    SM4::getInstance().setKey((unsigned char*)_key.data(), _key.size());
    SM4::getInstance().cbcEncrypt((unsigned char*)_cypherData.data(), deData.data(),
        _cypherData.size(), (unsigned char*)ivData.data(), 0);
    int padding = deData.at(_cypherData.size() - 1);
    int deLen = _cypherData.size() - padding;
    deData.resize(deLen);
    return deData;
}
bytes dev::readableKeyBytes(const std::string& _readableKey)
{
    if (_readableKey.length() != 32)
        BOOST_THROW_EXCEPTION(AESKeyLengthError() << errinfo_comment("Key must has 32 characters"));

    return bytesConstRef{(unsigned char*)_readableKey.c_str(), _readableKey.length()}.toBytes();
}