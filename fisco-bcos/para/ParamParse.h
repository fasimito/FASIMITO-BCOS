#pragma once
#include <libdevcore/FixedHash.h>
#include <libdevcore/easylog.h>
#include <libethcore/CommonJS.h>
#include <libethcore/Protocol.h>
#include <libnetwork/Host.h>
#include <libp2p/Service.h>
#include <boost/program_options.hpp>
#include <memory>

INITIALIZE_EASYLOGGINGPP
using namespace dev;
using namespace dev::p2p;
using namespace dev::eth;
class Params
{
public:
    Params() = delete;

    Params(boost::program_options::variables_map const& vm,
        boost::program_options::options_description const& option)
      : m_txSpeed(10)
    {
        initParams(vm, option);
    }

    void initParams(boost::program_options::variables_map const& vm,
        boost::program_options::options_description const&)
    {
        if (vm.count("txSpeed") || vm.count("t"))
        {
            m_txSpeed = vm["txSpeed"].as<float>();
        }

        if (vm.count("userCount") || vm.count("u"))
        {
            m_userCount = vm["userCount"].as<unsigned int>();
        }
    }

    float txSpeed() { return m_txSpeed; }
    float userCount() { return m_userCount; }

private:
    float m_txSpeed = 0;
    unsigned int m_userCount = 0;
};

static Params initCommandLine(int argc, const char* argv[])
{
    boost::program_options::options_description server_options("p2p module of FISCO-BCOS");
    server_options.add_options()("txSpeed,t",
        boost::program_options::value<float>()->default_value(10), "transaction generate speed")(
        "userCount,u", boost::program_options::value<unsigned int>()->default_value(1000),
        "the count of users")("help,h", "help of p2p module of FISCO-BCOS");

    boost::program_options::variables_map vm;
    try
    {
        boost::program_options::store(
            boost::program_options::parse_command_line(argc, argv, server_options), vm);
    }
    catch (...)
    {
        std::cout << "invalid input" << std::endl;
        exit(0);
    }
    /// help information
    if (vm.count("help") || vm.count("h"))
    {
        std::cout << server_options << std::endl;
        exit(0);
    }
    Params m_params(vm, server_options);
    return m_params;
}
