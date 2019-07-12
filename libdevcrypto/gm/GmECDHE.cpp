#include "libdevcrypto/ECDHE.h"
#include "libdevcrypto/Exceptions.h"
#include "libdevcrypto/Hash.h"

using namespace std;
using namespace dev;
using namespace dev::crypto;
bool ECDHE::agree(Public const&, Secret&) const
{
    BOOST_THROW_EXCEPTION(GmCryptoException() << errinfo_comment("GM not surrpot this algorithm"));
    return true;
}
