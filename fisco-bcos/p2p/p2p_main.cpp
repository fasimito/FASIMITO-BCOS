#include <libdevcore/easylog.h>
#include <libinitializer/Initializer.h>
#include <libinitializer/P2PInitializer.h>
#include <stdlib.h>

INITIALIZE_EASYLOGGINGPP

using namespace std;
using namespace dev;
using namespace dev::initializer;

int main()
{
    auto initialize = std::make_shared<Initializer>();
    initialize->init("./config.ini");
    auto p2pInitializer = initialize->p2pInitializer();
    auto p2pService = p2pInitializer->p2pService();
    uint32_t counter = 0;
    while (true)
    {
        std::shared_ptr<std::vector<std::string>> p_topics =
            std::shared_ptr<std::vector<std::string>>();
        std::string topic = "Topic" + to_string(counter++);
        p_topics->push_back(topic);
        P2PMSG_LOG(TRACE) << "Add topic periodically, now Topics[" << p_topics->size() - 1
                          << "]:" << topic;
        p2pService->setTopics(p_topics);
        LogInitializer::logRotateByTime();
        this_thread::sleep_for(chrono::milliseconds((rand() % 50) * 100));
    }
    return 0;
}
