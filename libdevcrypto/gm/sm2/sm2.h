#pragma once
#include <openssl/sm2.h>
#include <openssl/sm3.h>
#include <iostream>
#include <string>
#define CRYPTO_LOG(LEVEL) LOG(LEVEL) << "[CRYPTO] "
class SM2
{
public:
    bool genKey();
    std::string getPublicKey();
    std::string getPrivateKey();
    bool sign(const char* originalData, int originalDataLen, const std::string& privateKey,
        std::string& r, std::string& s);
    int verify(const std::string& signData, int signDataLen, const char* originalData,
        int originalDataLen, const std::string& publicKey);
    std::string priToPub(const std::string& privateKey);
    char* strlower(char* s);
    std::string ascii2hex(const char* chs, int len);
    static SM2& getInstance();

private:
    std::string publicKey;
    std::string privateKey;
};