#include "libdevcrypto/CryptoPP.h"
#include "libdevcrypto/Exceptions.h"
#include "libdevcrypto/Hash.h"
#include <cryptopp/eccrypto.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>
#include <libdevcore/Assertions.h>
#include <libdevcore/Guards.h>

using namespace dev;
using namespace dev::crypto;

static_assert(dev::Secret::size == 32, "Secret key must be 32 bytes.");
static_assert(dev::Public::size == 64, "Public key must be 64 bytes.");
static_assert(dev::Signature::size == 128, "Signature must be 128 bytes.");

namespace
{
    class Secp256k1PPCtx
    {
    public:
        CryptoPP::OID m_oid;

        std::mutex x_rng;
        CryptoPP::AutoSeededRandomPool m_rng;

        std::mutex x_params;
        CryptoPP::DL_GroupParameters_EC<CryptoPP::ECP> m_params;

        CryptoPP::DL_GroupParameters_EC<CryptoPP::ECP>::EllipticCurve m_curve;

        CryptoPP::Integer m_q;
        CryptoPP::Integer m_qs;

        static Secp256k1PPCtx& get()
        {
            static Secp256k1PPCtx ctx;
            return ctx;
        }

    private:
        Secp256k1PPCtx()
          : m_oid(CryptoPP::ASN1::secp256k1()),
            m_params(m_oid),
            m_curve(m_params.GetCurve()),
            m_q(m_params.GetGroupOrder()),
            m_qs(m_params.GetSubgroupOrder())
        {}
    };

    inline CryptoPP::ECP::Point publicToPoint(Public const&)
    {
        BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));
        return CryptoPP::ECP::Point(0, 0);
    }

    inline CryptoPP::Integer secretToExponent(Secret const& _s)
    {
        BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));

        return CryptoPP::Integer(_s.data(), Secret::size);
    }

}  // namespace

Secp256k1PP* Secp256k1PP::get()
{
    static Secp256k1PP s_this;
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));
    return &s_this;
}

void Secp256k1PP::encryptECIES(Public const&, bytes&)
{
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));
}

void Secp256k1PP::encryptECIES(Public const&, bytesConstRef, bytes&)
{
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));
}

bool Secp256k1PP::decryptECIES(Secret const&, bytes&)
{
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));

    return true;
}

bool Secp256k1PP::decryptECIES(Secret const&, bytesConstRef, bytes&)
{
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));

    return true;
}

void Secp256k1PP::encrypt(Public const&, bytes&)
{
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));
}

void Secp256k1PP::decrypt(Secret const&, bytes&)
{
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));
}

bool Secp256k1PP::agree(Secret const&, Public const&, Secret&)
{
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));
    return true;
}
