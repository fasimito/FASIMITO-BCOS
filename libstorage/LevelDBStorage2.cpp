/*
 * @CopyRight:
 * FISCO-BCOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FISCO-BCOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FISCO-BCOS.  If not, see <http://www.gnu.org/licenses/>
 * (c) 2016-2018 fisco-dev contributors.
 */
/** @file SealerPrecompiled.h
 *  @author ancelmo
 *  @date 20180921
 */

#include "LevelDBStorage2.h"
#include "Table.h"
#include "boost/archive/binary_iarchive.hpp"
#include "boost/archive/binary_oarchive.hpp"
#include "boost/serialization/map.hpp"
#include "boost/serialization/serialization.hpp"
#include "boost/serialization/vector.hpp"
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <libdevcore/BasicLevelDB.h>
#include <libdevcore/Common.h>
#include <libdevcore/FixedHash.h>
#include <libdevcore/Guards.h>
#include <libdevcore/RLP.h>
#include <libdevcore/easylog.h>
#include <memory>
#include <thread>

using namespace std;
using namespace dev;
using namespace leveldb;
using namespace dev::storage;

Entries::Ptr LevelDBStorage2::select(
    h256, int64_t, TableInfo::Ptr tableInfo, const std::string& key, Condition::Ptr condition)
{
    try
    {
        std::string entryKey = tableInfo->name;
        entryKey.append("_").append(key);

        std::string value;
        auto s = m_db->Get(ReadOptions(), Slice(entryKey), &value);
        if (!s.ok() && !s.IsNotFound())
        {
            STORAGE_LEVELDB_LOG(ERROR)
                << LOG_DESC("Query leveldb failed") << LOG_KV("status", s.ToString());

            BOOST_THROW_EXCEPTION(StorageException(-1, "Query leveldb exception:" + s.ToString()));
        }

        Entries::Ptr entries = std::make_shared<Entries>();
        if (!s.IsNotFound())
        {
            std::vector<std::map<std::string, std::string>> res;
            stringstream ss(value);
            boost::archive::binary_iarchive ia(ss);
            ia >> res;

            for (auto it = res.begin(); it != res.end(); ++it)
            {
                Entry::Ptr entry = std::make_shared<Entry>();

                for (auto valueIt = it->begin(); valueIt != it->end(); ++valueIt)
                {
                    entry->setField(valueIt->first, valueIt->second);
                }
                entry->setID(it->at(ID_FIELD));
                entry->setNum(it->at(NUM_FIELD));
                if (entry->getStatus() == Entry::Status::NORMAL && condition->process(entry))
                {
                    entry->setDirty(false);
                    entries->addEntry(entry);
                }
            }
        }

        return entries;
    }
    catch (std::exception& e)
    {
        STORAGE_LEVELDB_LOG(ERROR) << LOG_DESC("Query leveldb exception")
                                   << LOG_KV("msg", boost::diagnostic_information(e));
        BOOST_THROW_EXCEPTION(e);
    }

    return Entries::Ptr();
}

size_t LevelDBStorage2::commit(h256 hash, int64_t num, const std::vector<TableData::Ptr>& datas)
{
    try
    {
        auto hex = hash.hex();

        std::shared_ptr<dev::db::LevelDBWriteBatch> batch = m_db->createWriteBatch();
        for (size_t i = 0; i < datas.size(); ++i)
        {
            shared_ptr<map<string, vector<map<string, string>>>> key2value =
                make_shared<map<string, vector<map<string, string>>>>();

            auto tableInfo = datas[i]->info;

            processDirtyEntries(hash, num, key2value, tableInfo, datas[i]->dirtyEntries);
            processNewEntries(hash, num, key2value, tableInfo, datas[i]->newEntries);

            for (auto it : *key2value)
            {
                std::string entryKey = tableInfo->name + "_" + it.first;
                stringstream ss;
                boost::archive::binary_oarchive oa(ss);
                oa << it.second;
                batch->insertSlice(Slice(entryKey), Slice(ss.str()));
            }
        }

        leveldb::WriteOptions options;
        options.sync = false;
        m_db->Write(options, &batch->writeBatch());
        return datas.size();
    }
    catch (std::exception& e)
    {
        STORAGE_LEVELDB_LOG(ERROR) << LOG_DESC("Commit leveldb exception")
                                   << LOG_KV("msg", boost::diagnostic_information(e));
        BOOST_THROW_EXCEPTION(
            StorageException(-1, "Commit leveldb exception:" + boost::diagnostic_information(e)));
    }

    return 0;
}

bool LevelDBStorage2::onlyDirty()
{
    return false;
}

void LevelDBStorage2::setDB(std::shared_ptr<dev::db::BasicLevelDB> db)
{
    m_db = db;
}

void LevelDBStorage2::processNewEntries(h256, int64_t num,
    std::shared_ptr<std::map<std::string, std::vector<std::map<std::string, std::string>>>>
        key2value,
    TableInfo::Ptr tableInfo, Entries::Ptr entries)
{
    for (size_t j = 0; j < entries->size(); ++j)
    {
        auto entry = entries->get(j);
        auto key = entry->getField(tableInfo->key);

        auto it = key2value->find(key);
        if (it == key2value->end())
        {
            std::string entryKey = tableInfo->name;
            entryKey.append("_").append(key);

            std::string value;
            auto s = m_db->Get(ReadOptions(), Slice(entryKey), &value);
            if (!s.ok() && !s.IsNotFound())
            {
                STORAGE_LEVELDB_LOG(ERROR)
                    << LOG_DESC("Query leveldb failed") << LOG_KV("status", s.ToString());

                BOOST_THROW_EXCEPTION(
                    StorageException(-1, "Query leveldb exception:" + s.ToString()));
            }

            if (s.IsNotFound())
            {
                it = key2value->insert(make_pair(key, vector<map<string, string>>())).first;
            }
            else
            {
                std::vector<std::map<std::string, std::string>> res;
                stringstream ss(value);
                boost::archive::binary_iarchive ia(ss);
                ia >> res;
                it = key2value->emplace(key, res).first;
            }
        }

        std::map<std::string, std::string> value;
        for (auto& fieldIt : *(entry))
        {
            value[fieldIt.first] = fieldIt.second;
        }
        value[NUM_FIELD] = boost::lexical_cast<std::string>(num);
        value[ID_FIELD] = boost::lexical_cast<std::string>(entry->getID());

        it->second.push_back(value);
    }
}

void LevelDBStorage2::processDirtyEntries(h256, int64_t num,
    std::shared_ptr<std::map<std::string, std::vector<std::map<std::string, std::string>>>>
        key2value,
    TableInfo::Ptr tableInfo, Entries::Ptr entries)
{
    for (size_t j = 0; j < entries->size(); ++j)
    {
        auto entry = entries->get(j);
        auto key = entry->getField(tableInfo->key);

        auto it = key2value->find(key);
        if (it == key2value->end())
        {
            it = key2value->insert(make_pair(key, vector<map<string, string>>())).first;
        }
        std::map<std::string, std::string> value;
        for (auto& fieldIt : *(entry))
        {
            value[fieldIt.first] = fieldIt.second;
        }
        value[NUM_FIELD] = boost::lexical_cast<std::string>(num);
        value["_id_"] = boost::lexical_cast<std::string>(entry->getID());

        it->second.push_back(value);
    }
}
