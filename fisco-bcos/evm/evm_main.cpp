#include "EvmParams.h"
#include <fisco-bcos/Fake.h>
#include <libdevcore/Common.h>
#include <libethcore/ABI.h>
#include <libethcore/BlockHeader.h>
#include <libethcore/Transaction.h>
#include <libevm/ExtVMFace.h>
#include <libexecutive/Executive.h>
#include <libexecutive/StateFace.h>
#include <libmptstate/MPTState.h>
INITIALIZE_EASYLOGGINGPP
using namespace dev;
using namespace dev::eth;
using namespace dev::executive;
using namespace dev::mptstate;
using namespace dev::blockchain;
using namespace boost::property_tree;
static void FakeBlockHeader(BlockHeader& header, EvmParams const& param)
{
    header.setGasLimit(param.gasLimit());
    header.setNumber(param.blockNumber());
    header.setTimestamp(utcTime());
}

static void ExecuteTransaction(
    ExecutionResult& res, std::shared_ptr<MPTState> mptState, EnvInfo& info, Transaction const& tx)
{
    Executive executive(mptState, info, 0);
    executive.setResultRecipient(res);
    executive.initialize(tx);
    /// execute transaction
    if (!executive.execute())
    {
        /// Timer timer;
        executive.go();
        /// double execTime = timer.elapsed();
    }
    executive.finalize();
}

static void updateSender(
    std::shared_ptr<MPTState> mptState, Transaction& tx, EvmParams const& param)
{
    KeyPair key_pair = KeyPair::create();
    Address sender = toAddress(key_pair.pub());
    tx.forceSender(sender);
    mptState->addBalance(sender, param.transValue());
}

/// deploy contract
static void deployContract(
    std::shared_ptr<MPTState> mptState, EnvInfo& info, bytes const& code, EvmParams const& param)
{
    /// LOG(DEBUG) << "[evm_main] codeData: " << toHex(code);
    Transaction tx = Transaction(param.transValue(), param.gasPrice(), param.gas(), code, u256(0));
    updateSender(mptState, tx, param);
    ExecutionResult res;
    ExecuteTransaction(res, mptState, info, tx);
    EVMC_LOG(INFO) << "[evm_main/newAddress]:" << toHex(res.newAddress);
    EVMC_LOG(INFO) << "[evm_main/depositSize]:" << res.depositSize;
}

static void updateMptState(std::shared_ptr<MPTState> mptState, Input& input)
{
    Account account(u256(0), u256(0));
    account.setCode(bytes{input.codeData});
    AccountMap map;
    map[input.addr] = account;
    mptState->getState().populateFrom(map);
}

/// call contract
static void callTransaction(
    std::shared_ptr<MPTState> mptState, EnvInfo& info, Input& input, EvmParams const& param)
{
    if (input.codeData.size() > 0)
    {
        KeyPair key_pair = KeyPair::create();
        input.addr = toAddress(key_pair.pub());
        updateMptState(mptState, input);
    }
    ContractABI abi;
    bytes inputData = abi.abiIn(input.inputCall);
    EVMC_LOG(INFO) << "[evm_main/callTransaction/Parms]: " << toHex(inputData);
    Transaction tx = Transaction(
        param.transValue(), param.gasPrice(), param.gas(), input.addr, inputData, u256(0));
    updateSender(mptState, tx, param);
    ExecutionResult res;
    ExecuteTransaction(res, mptState, info, tx);
    bytes output = std::move(res.output);
    std::string result;
    abi.abiOut(ref(output), result);
    EVMC_LOG(INFO) << "[evm_main/callTransaction/output]: " << toHex(output);
    EVMC_LOG(INFO) << "[evm_main/callTransaction/result string]: " << result;
}

int main()
{
    /// init configuration
    ptree pt;
    read_ini("config.ini", pt);
    EvmParams param(pt);
    /// fake blockHeader
    BlockHeader header;
    FakeBlockHeader(header, param);
    /// Fake envInfo
    std::shared_ptr<FakeBlockChain> blockChain = std::make_shared<FakeBlockChain>();
    EnvInfo envInfo(header, boost::bind(&FakeBlockChain::numberHash, blockChain, _1), u256(0));
    /// init state
    std::shared_ptr<MPTState> mptState = std::make_shared<MPTState>(
        u256(0), MPTState::openDB("./", sha3("0x1234")), BaseState::Empty);
    /// test deploy
    for (size_t i = 0; i < param.code().size(); i++)
    {
        EVMC_LOG(INFO) << "=======[evm_main/BEGIN deploy Contract/index]:" << i << "=======";
        deployContract(mptState, envInfo, param.code()[i], param);
    }
    /// test callfunctions
    for (size_t i = 0; i < param.input().size(); i++)
    {
        EVMC_LOG(INFO) << "=======[evm_main/BEGIN call transaction/index]:" << i << "=======";
        callTransaction(mptState, envInfo, param.input()[i], param);
    }
    return 0;
}
