#include "AES.h"
#include "Exceptions.h"
#include <cryptopp/aes.h>
#include <cryptopp/filters.h>
#include <cryptopp/modes.h>
#include <cryptopp/pwdbased.h>
#include <cryptopp/sha.h>
#include <libdevcore/easylog.h>
#include <stdlib.h>
#include <string>

using namespace std;
using namespace dev;
using namespace dev::crypto;
using namespace std;

bytes dev::aesCBCEncrypt(bytesConstRef _plainData, bytesConstRef _key)
{
    bytesConstRef ivData = _key.cropped(0, 16);
    string cipherData;
    CryptoPP::AES::Encryption aesEncryption(_key.data(), _key.size());
    CryptoPP::CBC_Mode_ExternalCipher::Encryption cbcEncryption(aesEncryption, ivData.data());
    CryptoPP::StreamTransformationFilter stfEncryptor(
        cbcEncryption, new CryptoPP::StringSink(cipherData));
    stfEncryptor.Put(_plainData.data(), _plainData.size());
    stfEncryptor.MessageEnd();
    return asBytes(cipherData);
}

bytes dev::aesCBCDecrypt(bytesConstRef _cypherData, bytesConstRef _key)
{
    bytes ivData = _key.cropped(0, 16).toBytes();
    string decryptedData;
    CryptoPP::AES::Decryption aesDecryption(_key.data(), _key.size());
    CryptoPP::CBC_Mode_ExternalCipher::Decryption cbcDecryption(aesDecryption, ref(ivData).data());
    CryptoPP::StreamTransformationFilter stfDecryptor(
        cbcDecryption, new CryptoPP::StringSink(decryptedData));
    stfDecryptor.Put(_cypherData.data(), _cypherData.size());
    stfDecryptor.MessageEnd();
    return asBytes(decryptedData);
}

bytes dev::readableKeyBytes(const std::string& _readableKey)
{
    if (_readableKey.length() != 32)
        BOOST_THROW_EXCEPTION(AESKeyLengthError() << errinfo_comment("Key must has 32 characters"));
    return bytesConstRef{(unsigned char*)_readableKey.c_str(), _readableKey.length()}.toBytes();
}