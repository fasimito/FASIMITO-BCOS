#include "libdevcrypto/AES.h"
#include "libdevcrypto/Common.h"
#include "libdevcrypto/CryptoPP.h"
#include "libdevcrypto/ECDHE.h"
#include "libdevcrypto/Exceptions.h"
#include "libdevcrypto/Hash.h"
#include "libdevcrypto/gm/sm2/sm2.h"
#include <cryptopp/modes.h>
#include <cryptopp/pwdbased.h>
#include <libdevcore/Guards.h>
#include <libdevcore/RLP.h>
#include <libethcore/Exceptions.h>
#include <secp256k1.h>
#include <secp256k1_recovery.h>
#include <secp256k1_sha256.h>
using namespace std;
using namespace dev;
using namespace dev::crypto;


SignatureStruct::SignatureStruct(Signature const& _s)
{
    *(Signature*)this = _s;
}

SignatureStruct::SignatureStruct(h256 const& _r, h256 const& _s, VType _v) : r(_r), s(_s), v(_v) {}
SignatureStruct::SignatureStruct(u256 const& _r, u256 const& _s, NumberVType _v)
{
    r = _r;
    s = _s;
    v = _v;
}

pair<bool, bytes> SignatureStruct::ecRecover(bytesConstRef _in)
{
    struct
    {
        h256 hash;
        h512 v;
        h256 r;
        h256 s;
    } in;
    memcpy(&in, _in.data(), min(_in.size(), sizeof(in)));

    SignatureStruct sig(in.r, in.s, in.v);
    if (sig.isValid())
    {
        try
        {
            if (Public rec = recover(sig, in.hash))
            {
                h256 ret = dev::sha3(rec);
                memset(ret.data(), 0, 12);
                return {true, ret.asBytes()};
            }
        }
        catch (...)
        {
        }
    }
    return {true, {}};
}


void SignatureStruct::encode(RLPStream& _s) const noexcept
{
    _s << (VType)(v) << (u256)r << (u256)s;
}


void SignatureStruct::check() const noexcept
{
    if (!v)
        BOOST_THROW_EXCEPTION(eth::InvalidSignature());
}

namespace
{

    secp256k1_context const* getCtx()
    {
        static std::unique_ptr<secp256k1_context, decltype(&secp256k1_context_destroy)> s_ctx{
            secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY),
            &secp256k1_context_destroy};
        return s_ctx.get();
    }

}

bool dev::SignatureStruct::isValid() const noexcept
{
    static const h256 s_max{"0xfffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141"};
    static const h256 s_zero;

    return (v >= h512(1) && r > s_zero && s > s_zero && r < s_max && s < s_max);
}

/**
 * @brief : obtain public key according to secret key
 * @param _secret : the data of secret key
 * @return Public : created public key; if create failed, assertion failed
 */
Public dev::toPublic(Secret const& _secret)
{
    string pri = toHex(bytesConstRef{_secret.data(), 32});
    string pub = SM2::getInstance().priToPub(pri);
    return Public(fromHex(pub));
}

/**
 * @brief obtain address from public key
 *        by adding the last 20Bytes of sha3(public key)
 * @param _public : the public key need to convert to address
 * @return Address : the converted address
 */
Address dev::toAddress(Public const& _public)
{
    return right160(sha3(_public.ref()));
}

Address dev::toAddress(Secret const& _secret)
{
    return toAddress(toPublic(_secret));
}

/**
 * @brief : 1.serialize (_from address, nonce) into rlpStream
 *          2.calculate the sha3 of serialized (_from, nonce)
 *          3.obtaining the last 20Bytes of the sha3 as address
 *          (mainly used for contract address generating)
 * @param _from : address that sending this transaction
 * @param _nonce : random number
 * @return Address : generated address
 */
Address dev::toAddress(Address const& _from, u256 const& _nonce)
{
    return right160(sha3(rlpList(_from, _nonce)));
}

/**
 * @brief : encrypt plain text with public key
 * @param _k : public key
 * @param _plain : plain text need to be encrypted
 * @param o_cipher : encrypted ciper text
 */
void dev::encrypt(Public const&, bytesConstRef, bytes&)
{
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));
}

/**
 * @brief : decrypt ciper text with secret key
 * @param _k : private key used to decrypt
 * @param _cipher : ciper text
 * @param o_plaintext : decrypted plain text
 * @return true : decrypt succeed
 * @return false : decrypt failed(maybe key or ciper text is invalid)
 */
bool dev::decrypt(Secret const&, bytesConstRef, bytes&)
{
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));
    return true;
}

void dev::encryptECIES(Public const&, bytesConstRef, bytes&)
{
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));
}

void dev::encryptECIES(Public const&, bytesConstRef, bytesConstRef, bytes&)
{
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));
}

bool dev::decryptECIES(Secret const& _k, bytesConstRef _cipher, bytes& o_plaintext)
{
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));
    return decryptECIES(_k, bytesConstRef(), _cipher, o_plaintext);
}

bool dev::decryptECIES(Secret const&, bytesConstRef, bytesConstRef, bytes&)
{
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));
    return true;
}

void dev::encryptSym(Secret const&, bytesConstRef, bytes&)
{
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));
}

bool dev::decryptSym(Secret const&, bytesConstRef, bytes&)
{
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));
    return true;
}

std::pair<bytes, h128> dev::encryptSymNoAuth(SecureFixedHash<16> const& _k, bytesConstRef _plain)
{
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));
    h128 iv(Nonce::get().makeInsecure());
    return make_pair(encryptSymNoAuth(_k, iv, _plain), iv);
}

bytes dev::encryptAES128CTR(bytesConstRef, h128 const&, bytesConstRef)
{
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));
    return bytes();
}

bytesSec dev::decryptAES128CTR(bytesConstRef, h128 const&, bytesConstRef)
{
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));
    return bytesSec();
}

Public dev::recover(Signature const& _sig, h256 const& _message)
{
    SignatureStruct sign(_sig);
    if (!sign.isValid())
    {
        return Public{};
    }
    if (verify(sign.v, _sig, _message))
    {
        return sign.v;
    }
    return Public{};
    // return sign.pub;
}

Signature dev::sign(Secret const& _k, h256 const& _hash)
{
    string pri = toHex(bytesConstRef{_k.data(), 32});
    string r = "", s = "";
    if (!SM2::getInstance().sign((const char*)_hash.data(), h256::size, pri, r, s))
    {
        return Signature{};
    }
    // std::cout << "Gmcommon.cpp line 255 Secret====" << pri << std::endl;
    string pub = SM2::getInstance().priToPub(pri);
    // std::cout <<"_hash:"<<toHex(_hash.asBytes())<<"gmSign:"<< r + s + pub;
    bytes byteSign = fromHex(r + s + pub);
    // std::cout <<"sign toHex:"<<toHex(byteSign)<<" sign toHexLen:"<<toHex(byteSign).length();
    return Signature{byteSign};
}


KeyPair KeyPair::create()
{
    while (true)
    {
        KeyPair keyPair(Secret::random());
        if (keyPair.address())
            return keyPair;
    }
}


Secret Nonce::next()
{
    Guard l(x_value);
    if (!m_value)
    {
        m_value = Secret::random();
        if (!m_value)
            BOOST_THROW_EXCEPTION(InvalidState());
    }
    m_value = sha3Secure(m_value.ref());
    return sha3(~m_value);
}

bool dev::verify(Public const& _p, Signature const& _s, h256 const& _hash)
{
    string signData = toHex(_s.asBytes());
    string pub = toHex(_p.asBytes());
    pub = "04" + pub;
    bool lresult = SM2::getInstance().verify(
        signData, signData.length(), (const char*)_hash.data(), h256::size, pub);
    return lresult;
}


bool dev::crypto::ecdh::agree(Secret const&, Public const&, Secret&)
{
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));
    return true;
}

bytes ecies::kdf(Secret const&, bytes const&, unsigned)
{
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));
    return bytes();
}
