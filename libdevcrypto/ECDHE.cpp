#include "ECDHE.h"
#include "CryptoPP.h"
#include "libdevcrypto/Hash.h"

using namespace std;
using namespace dev;
using namespace dev::crypto;
bool ECDHE::agree(Public const& _remote, Secret& o_sharedSecret) const
{
    if (m_remoteEphemeral)
        BOOST_THROW_EXCEPTION(InvalidState());
    m_remoteEphemeral = _remote;
    return Secp256k1PP::get()->agree(m_ephemeral.secret(), m_remoteEphemeral, o_sharedSecret);
}
