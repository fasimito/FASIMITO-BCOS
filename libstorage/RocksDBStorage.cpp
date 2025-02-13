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
/** @file RocksDBStorage.cpp
 *  @author xingqiangbai
 *  @date 20180423
 */

#include "RocksDBStorage.h"
#include "StorageException.h"
#include "Table.h"
#include "boost/archive/binary_iarchive.hpp"
#include "boost/archive/binary_oarchive.hpp"
#include "boost/serialization/map.hpp"
#include "boost/serialization/serialization.hpp"
#include "boost/serialization/vector.hpp"
#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "rocksdb/write_batch.h"
#include <libdevcore/Common.h>
#include <libdevcore/FixedHash.h>
#include <libdevcore/Guards.h>
#include <libdevcore/RLP.h>
#include <libdevcore/easylog.h>
#include <tbb/parallel_for.h>
#include <memory>
#include <thread>

using namespace std;
using namespace dev;
using namespace dev::storage;
using namespace rocksdb;

Entries::Ptr RocksDBStorage::select(
    h256, int64_t, TableInfo::Ptr tableInfo, const string& key, Condition::Ptr condition)
{
    try
    {
        string entryKey = tableInfo->name;
        entryKey.append("_").append(key);

        string value;
        auto s = m_db->Get(ReadOptions(), Slice(std::move(entryKey)), &value);
        if (!s.ok() && !s.IsNotFound())
        {
            STORAGE_ROCKSDB_LOG(ERROR)
                << LOG_DESC("Query rocksdb failed") << LOG_KV("status", s.ToString());

            BOOST_THROW_EXCEPTION(StorageException(-1, "Query rocksdb exception:" + s.ToString()));
        }

        Entries::Ptr entries = make_shared<Entries>();
        if (!s.IsNotFound())
        {
            vector<map<string, string>> res;
            stringstream ss(value);
            boost::archive::binary_iarchive ia(ss);
            ia >> res;

            for (auto it = res.begin(); it != res.end(); ++it)
            {
                Entry::Ptr entry = make_shared<Entry>();

                for (auto valueIt = it->begin(); valueIt != it->end(); ++valueIt)
                {
                    entry->setField(valueIt->first, valueIt->second);
                }
                entry->setID(it->at(ID_FIELD));
                entry->setNum(it->at(NUM_FIELD));

                auto statusIt = it->find(STATUS);
                if (statusIt != it->end())
                {
                    entry->setStatus(statusIt->second);
                }

                if (entry->getStatus() == Entry::Status::NORMAL && condition->process(entry))
                {
                    entry->setDirty(false);
                    entries->addEntry(entry);
                }
            }
        }

        return entries;
    }
    catch (exception& e)
    {
        STORAGE_ROCKSDB_LOG(ERROR) << LOG_DESC("Query rocksdb exception")
                                   << LOG_KV("msg", boost::diagnostic_information(e));

        BOOST_THROW_EXCEPTION(e);
    }

    return Entries::Ptr();
}

size_t RocksDBStorage::commit(h256 hash, int64_t num, const vector<TableData::Ptr>& datas)
{
    try
    {
        auto start_time = utcTime();

        auto hex = hash.hex();
        WriteBatch batch;
        tbb::parallel_for(tbb::blocked_range<size_t>(0, datas.size()),
            [&](const tbb::blocked_range<size_t>& range) {
                for (size_t i = range.begin(); i < range.end(); ++i)
                {
                    shared_ptr<map<string, vector<map<string, string>>>> key2value =
                        make_shared<map<string, vector<map<string, string>>>>();

                    auto tableInfo = datas[i]->info;

                    processDirtyEntries(num, key2value, tableInfo, datas[i]->dirtyEntries);
                    processNewEntries(num, key2value, tableInfo, datas[i]->newEntries);

                    for (auto it : *key2value)
                    {
                        string entryKey = tableInfo->name + "_" + it.first;
                        stringstream ss;
                        boost::archive::binary_oarchive oa(ss);
                        oa << it.second;
                        {
                            tbb::spin_mutex::scoped_lock lock(m_writeBatchMutex);
                            batch.Put(Slice(std::move(entryKey)), Slice(ss.str()));
                        }
                    }
                }
            });
        auto encode_time_cost = utcTime();

        WriteOptions options;
        options.sync = false;
        m_db->Write(options, &batch);
        auto writeDB_time_cost = utcTime();
        STORAGE_ROCKSDB_LOG(DEBUG)
            << LOG_BADGE("Commit") << LOG_DESC("Write to db")
            << LOG_KV("encodeTimeCost", encode_time_cost - start_time)
            << LOG_KV("writeDBTimeCost", writeDB_time_cost - encode_time_cost)
            << LOG_KV("totalTimeCost", utcTime() - start_time);

        return datas.size();
    }
    catch (exception& e)
    {
        STORAGE_ROCKSDB_LOG(ERROR) << LOG_DESC("Commit rocksdb exception")
                                   << LOG_KV("msg", boost::diagnostic_information(e));
        BOOST_THROW_EXCEPTION(e);
    }

    return 0;
}

bool RocksDBStorage::onlyDirty()
{
    return false;
}

void RocksDBStorage::setDB(shared_ptr<rocksdb::DB> db)
{
    m_db = db;
}

void RocksDBStorage::processNewEntries(int64_t num,
    shared_ptr<map<string, vector<map<string, string>>>> key2value, TableInfo::Ptr tableInfo,
    Entries::Ptr entries)
{
    for (size_t j = 0; j < entries->size(); ++j)
    {
        auto entry = entries->get(j);
        auto key = entry->getField(tableInfo->key);

        auto it = map<string, vector<map<string, string>>>::iterator();
        if (entry->force())
        {
            it = key2value->insert(make_pair(key, vector<map<string, string>>())).first;
        }
        else
        {
            it = key2value->find(key);
            if (it == key2value->end())
            {
                string entryKey = tableInfo->name;
                entryKey.append("_").append(key);

                string value;
                auto s = m_db->Get(ReadOptions(), Slice(std::move(entryKey)), &value);
                if (!s.ok() && !s.IsNotFound())
                {
                    STORAGE_ROCKSDB_LOG(ERROR)
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
                    vector<map<string, string>> res;
                    stringstream ss(value);
                    boost::archive::binary_iarchive ia(ss);
                    ia >> res;
                    it = key2value->emplace(key, res).first;
                }
            }
        }

        map<string, string> value;
        for (auto& fieldIt : *(entry))
        {
            value[fieldIt.first] = fieldIt.second;
        }
        value[NUM_FIELD] = boost::lexical_cast<string>(num);
        value[ID_FIELD] = boost::lexical_cast<string>(entry->getID());
        value[STATUS] = boost::lexical_cast<string>(entry->getStatus());
        it->second.push_back(value);
    }
}

void RocksDBStorage::processDirtyEntries(int64_t num,
    shared_ptr<map<string, vector<map<string, string>>>> key2value, TableInfo::Ptr tableInfo,
    Entries::Ptr entries)
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

        map<string, string> value;
        for (auto& fieldIt : *(entry))
        {
            value[fieldIt.first] = fieldIt.second;
        }
        value[NUM_FIELD] = boost::lexical_cast<string>(num);
        value[ID_FIELD] = boost::lexical_cast<string>(entry->getID());
        value[STATUS] = boost::lexical_cast<string>(entry->getStatus());
        it->second.push_back(value);
    }
}
