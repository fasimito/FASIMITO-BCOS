#include "libinitializer/Initializer.h"
#include "libstorage/MemoryTableFactory.h"
#include <leveldb/db.h>
#include <libdevcore/BasicLevelDB.h>
#include <libdevcore/Common.h>
#include <libdevcore/easylog.h>
#include <libstorage/LevelDBStorage.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/program_options.hpp>
INITIALIZE_EASYLOGGINGPP

using namespace std;
using namespace dev;
using namespace boost;
using namespace dev::storage;
using namespace dev::initializer;
namespace po = boost::program_options;

po::options_description main_options("Main for mini-storage");

po::variables_map initCommandLine(int argc, const char* argv[])
{
    main_options.add_options()("help,h", "help of mini-storage")("createTable,c",
        po::value<vector<string>>()->multitoken(), "[TableName] [KeyField] [ValueField]")(
        "path,p", po::value<string>()->default_value("data/"), "[LevelDB path]")(
        "select,s", po::value<vector<string>>()->multitoken(), "[TableName] [priKey]")("update,u",
        po::value<vector<string>>()->multitoken(), "[TableName] [priKey] [Key] [NewValue]")(
        "insert,i", po::value<vector<string>>()->multitoken(),
        "[TableName] [priKey] [Key]:[Value],...,[Key]:[Value]")(
        "remove,r", po::value<vector<string>>()->multitoken(), "[TableName] [priKey]");
    po::variables_map vm;
    try
    {
        po::store(po::parse_command_line(argc, argv, main_options), vm);
        po::notify(vm);
    }
    catch (...)
    {
        std::cout << "invalid input" << std::endl;
        exit(0);
    }
    /// help information
    if (vm.count("help") || vm.count("h"))
    {
        std::cout << main_options << std::endl;
        exit(0);
    }

    return vm;
}

void help()
{
    std::cout << main_options << std::endl;
}

void printEntries(Entries::ConstPtr entries)
{
    if (entries->size() == 0)
    {
        cout << "--------------Empty!--------------" << endl;
        return;
    }
    cout << "============================" << endl;
    for (size_t i = 0; i < entries->size(); ++i)
    {
        cout << "***************" << i << "***************" << endl;
        auto data = entries->get(i);
        for (auto& it : *data)
        {
            cout << "[ " << it.first << " ]:[ " << it.second << " ]" << endl;
        }
    }
    cout << "============================" << endl;
}

int main(int argc, const char* argv[])
{
    // init log
    boost::property_tree::ptree pt;
    auto logInitializer = std::make_shared<LogInitializer>();
    logInitializer->initLog(pt);
    /// init params
    auto params = initCommandLine(argc, argv);
    auto storagePath = params["path"].as<string>();
    cout << "LevelDB path : " << storagePath << endl;
    filesystem::create_directories(storagePath);
    leveldb::Options option;
    option.create_if_missing = true;
    option.max_open_files = 1000;
    dev::db::BasicLevelDB* dbPtr = NULL;
    leveldb::Status s = dev::db::BasicLevelDB::Open(option, storagePath, &dbPtr);
    if (!s.ok())
    {
        cerr << "Open storage leveldb error: " << s.ToString() << endl;
        return -1;
    }

    auto storageDB = std::shared_ptr<dev::db::BasicLevelDB>(dbPtr);
    auto storage = std::make_shared<dev::storage::LevelDBStorage>();
    storage->setDB(storageDB);
    dev::storage::MemoryTableFactory::Ptr memoryTableFactory =
        std::make_shared<dev::storage::MemoryTableFactory>();
    memoryTableFactory->setStateStorage(storage);
    memoryTableFactory->setBlockHash(h256(0));
    memoryTableFactory->setBlockNum(0);

    if (params.count("createTable") || params.count("c"))
    {
        auto& p = params["createTable"].as<vector<string>>();
        cout << "createTable " << p << " || params num : " << p.size() << endl;
        if (p.size() == 3u)
        {
            auto table = memoryTableFactory->createTable(p[0], p[1], p[2], true);
            if (table)
            {
                cout << "KeyField:[" << p[1] << "]" << endl;
                cout << "ValueField:[" << p[2] << "]" << endl;
                cout << "createTable [" << p[0] << "] success!" << endl;
            }
            memoryTableFactory->commitDB(h256(0), 1);
            return 0;
        }
    }
    else if (params.count("select") || params.count("s"))
    {
        auto& p = params["select"].as<vector<string>>();
        cout << "select " << p << " || params num : " << p.size() << endl;
        if (p.size() == 2u)
        {
            auto table = memoryTableFactory->openTable(p[0]);
            if (table)
            {
                cout << "open Table [" << p[0] << "] success!" << endl;
                auto entries = table->select(p[1], table->newCondition());
                printEntries(entries);
            }
            return 0;
        }
    }
    else if (params.count("update") || params.count("u"))
    {
        auto& p = params["update"].as<vector<string>>();
        cout << "update " << p << " || params num : " << p.size() << endl;
        if (p.size() == 4u)
        {
            auto table = memoryTableFactory->openTable(p[0]);
            if (table)
            {
                cout << "open Table [" << p[0] << "] success!" << endl;
                auto entry = table->newEntry();
                entry->setField(p[2], p[3]);
                table->update(p[1], entry, table->newCondition());
            }
            memoryTableFactory->commitDB(h256(0), 1);
            return 0;
        }
    }
    else if (params.count("insert") || params.count("i"))
    {
        auto& p = params["insert"].as<vector<string>>();
        cout << "insert " << p << " || params num : " << p.size() << endl;
        if (p.size() == 3u)
        {
            auto table = memoryTableFactory->openTable(p[0]);
            if (table)
            {
                cout << "open Table [" << p[0] << "] success!" << endl;
                auto entry = table->newEntry();
                vector<string> KVs;
                boost::split(KVs, p[2], boost::is_any_of(","));
                for (auto& kv : KVs)
                {
                    vector<string> KV;
                    boost::split(KV, kv, boost::is_any_of(":"));
                    entry->setField(KV[0], KV[1]);
                }
                table->insert(p[1], entry);
            }
            memoryTableFactory->commitDB(h256(0), 1);
            return 0;
        }
    }
    else if (params.count("remove") || params.count("r"))
    {
        auto& p = params["remove"].as<vector<string>>();
        cout << "remove " << p << " || params num : " << p.size() << endl;
        if (p.size() == 2u)
        {
            auto table = memoryTableFactory->openTable(p[0]);
            if (table)
            {
                cout << "open Table [" << p[0] << "] success!" << endl;
                table->remove(p[1], table->newCondition());
            }
            memoryTableFactory->commitDB(h256(0), 1);
            return 0;
        }
    }
    help();
    return -1;
}
