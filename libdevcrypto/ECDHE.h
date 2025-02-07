#pragma once

#include "Common.h"

namespace dev
{
namespace crypto
{
/**
 * @brief Derive DH shared secret from EC keypairs.
 * As ephemeral keys are single-use, agreement is limited to a single occurrence.
 */
class ECDHE
{
public:
    /// Constructor (pass public key for ingress exchange).
    ECDHE() : m_ephemeral(KeyPair::create()){};
    ECDHE(KeyPair k) : m_ephemeral(k){};
    /// Public key sent to remote.
    Public pubkey() { return m_ephemeral.pub(); }

    Secret seckey() { return m_ephemeral.secret(); }

    /// Input public key for dh agreement, output generated shared secret.
    bool agree(Public const& _remoteEphemeral, Secret& o_sharedSecret) const;

protected:
    KeyPair m_ephemeral;               ///< Ephemeral keypair; generated.
    mutable Public m_remoteEphemeral;  ///< Public key of remote; parameter. Set once when agree is
                                       ///< called, otherwise immutable.
};

}  // namespace crypto
}  // namespace dev
