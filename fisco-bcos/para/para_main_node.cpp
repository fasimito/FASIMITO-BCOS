#include "ParamParse.h"
#include <libblockchain/BlockChainImp.h>
#include <libblockverifier/BlockVerifier.h>
#include <libdevcore/easylog.h>
#include <libethcore/ABI.h>
#include <libethcore/Protocol.h>
#include <libinitializer/Initializer.h>
#include <libinitializer/LedgerInitializer.h>
#include <libinitializer/P2PInitializer.h>
#include <libinitializer/SecureInitializer.h>
#include <libledger/DBInitializer.h>
#include <libledger/LedgerManager.h>
#include <libtxpool/TxPool.h>
#include <unistd.h>
#include <chrono>
#include <ctime>
#include <random>
#include <string>
#include <thread>

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::ledger;
using namespace dev::initializer;
using namespace dev::txpool;
using namespace dev::blockverifier;
using namespace dev::blockchain;

void generateUserAddTx(std::shared_ptr<LedgerManager> ledgerManager, size_t _userNum)
{
    Secret sec = KeyPair::create().secret();
    // Generate user + receiver = _userNum
    for (size_t i = 0; i < _userNum; i++)
    {
        try
        {
            u256 value = 0;
            u256 gasPrice = 0;
            u256 gas = 10000000;
            Address dest = Address(0x5002);
            string user = to_string(i);
            u256 money = 1000000000;
            dev::eth::ContractABI abi;
            bytes data =
                abi.abiIn("userSave(string,uint256)", user, money);  // add 1000000000 to user i
            u256 nonce = u256(utcTime());
            Transaction tx(value, gasPrice, gas, dest, data, nonce);
            Signature sig = sign(sec, tx.sha3(WithoutSignature));

            for (auto group : ledgerManager->getGroupList())
            {
                tx.setBlockLimit(u256(ledgerManager->blockChain(group)->number()) + 250);
                tx.updateSignature(SignatureStruct(sig));
                ledgerManager->txPool(group)->submit(tx);
            }
        }
        catch (std::exception& e)
        {
            LOG(TRACE) << "[SYNC_MAIN]: submit transaction failed: [EINFO]:  "
                       << boost::diagnostic_information(e);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

static void createTx(std::shared_ptr<LedgerManager> ledgerManager, float txSpeed,
    unsigned int userCount, KeyPair const&)
{
    Secret sec = KeyPair::create().secret();
    u256 maxBlockLimit = u256(500);
    uint16_t sleep_interval = (uint16_t)(1000.0 / txSpeed);
    random_device rd;
    mt19937 mt(rd());
    uniform_int_distribution<int> dist(0, userCount);

    while (true)
    {
        for (auto group : ledgerManager->getGroupList())
        {
            auto hasGenerated = false;
            while (ledgerManager->blockChain(group)->number() <= 0)
            {
                if (!hasGenerated)
                {
                    generateUserAddTx(ledgerManager, userCount);
                    hasGenerated = true;
                }
                std::this_thread::yield();
            }

            try
            {
                u256 value = 0;
                u256 gasPrice = 0;
                u256 gas = 10000000;
                Address dest = Address(0x5002);
                string userFrom;
                string userTo;

                userFrom = to_string(dist(mt));
                userTo = to_string(dist(mt));

                u256 money = 1;
                dev::eth::ContractABI abi;
                bytes data = abi.abiIn("userTransfer(string,string,uint256)", userFrom, userTo,
                    money);  // add 1000000000 to user i
                u256 nonce = u256(utcTime() + rand());

                Transaction tx(value, gasPrice, gas, dest, data, nonce);
                // sec = KeyPair::create().secret();
                Signature sig = sign(sec, tx.sha3(WithoutSignature));

                tx.setBlockLimit(u256(ledgerManager->blockChain(group)->number()) + maxBlockLimit);
                tx.updateSignature(SignatureStruct(sig));
                ledgerManager->txPool(group)->submit(tx);
            }

            catch (std::exception& e)
            {
                LOG(TRACE) << "[PARA_MAIN_NODE]: submit transaction failed: [EINFO]:  "
                           << boost::diagnostic_information(e);
            }
        }
        LogInitializer::logRotateByTime();
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_interval));
    }
}

static void startConsensus(Params& params)
{
    ///< initialize component
    auto initialize = std::make_shared<Initializer>();
    initialize->init("./config.ini");

    auto secureInitializer = initialize->secureInitializer();
    KeyPair key_pair = secureInitializer->keyPair();
    auto ledgerManager = initialize->ledgerInitializer()->ledgerManager();

    /// create transaction
    createTx(ledgerManager, params.txSpeed(), params.userCount(), key_pair);
}

int main(int argc, const char* argv[])
{
    Params params = initCommandLine(argc, argv);
    startConsensus(params);
}
