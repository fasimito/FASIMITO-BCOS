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
 * (c) 2016-2019 fisco-dev contributors.
 */
/**
 * @brief : implement basic interface to access RocksDB
 * @author: yujiechen
 * @date: 2019-06-22
 */

#include "BasicRocksDB.h"
#include <libdevcore/Exceptions.h>
using namespace dev;
using namespace rocksdb;

namespace dev
{
namespace db
{
/**
 * @brief: open rocksDB
 *
 * @param options: options used to open the rocksDB
 * @param dbname: db name
 * @return std::shared_ptr<rocksdb::DB>:
 * 1. open successfully: return the DB handler
 * 2. open failed: throw exception(OpenDBFailed)
 */
std::shared_ptr<rocksdb::DB> BasicRocksDB::Open(const Options& options, const std::string& dbname)
{
    ROCKSDB_LOG(INFO) << LOG_DESC("open rocksDB handler");
    DB* db = nullptr;
    auto status = DB::Open(options, dbname, &db);
    checkStatus(status, dbname);
    m_db.reset(db);
    return m_db;
}

Status BasicRocksDB::Get(ReadOptions const& options, std::string const& key, std::string& value)
{
    assert(m_db);
    auto status = m_db->Get(options, Slice(std::move(key)), &value);
    checkStatus(status);
    // decrypt value
    if (m_decryptHandler && !value.empty())
    {
        m_decryptHandler(value);
    }
    return status;
}

Status BasicRocksDB::BatchPut(WriteBatch& batch, std::string const& key, std::string const& value)
{
    auto status = batch.Put(Slice(std::move(key)), Slice(value));
    checkStatus(status);
    return status;
}

Status BasicRocksDB::PutWithLock(
    WriteBatch& batch, std::string const& key, std::string& value, tbb::spin_mutex& mutex)
{
    // encrypt value
    if (m_encryptHandler)
    {
        m_encryptHandler(value);
    }
    // put handled value into the batch
    tbb::spin_mutex::scoped_lock lock(mutex);
    return BatchPut(batch, key, value);
}

Status BasicRocksDB::Put(WriteBatch& batch, std::string const& key, std::string& value)
{
    // encrypt value
    if (m_encryptHandler)
    {
        m_encryptHandler(value);
    }
    // put handled value into the batch
    return BatchPut(batch, key, value);
}

void BasicRocksDB::checkStatus(Status const& status, std::string const& path)
{
    if (status.ok() || status.IsNotFound())
    {
        return;
    }
    std::string errorInfo = "access rocksDB failed, status: " + status.ToString();
    if (!path.empty())
    {
        errorInfo = errorInfo + ", path:" + path;
    }
    // fatal exception
    if (status.IsIOError() || status.IsCorruption() || status.IsNoSpace() ||
        status.IsNotSupported() || status.IsShutdownInProgress())
    {
        ROCKSDB_LOG(ERROR) << LOG_DESC(errorInfo);
        BOOST_THROW_EXCEPTION(DatabaseError() << errinfo_comment(errorInfo));
    }
    // exception that can be recovered by retry
    // statuses are: Busy, TimedOut, TryAgain, Aborted, MergeInProgress, IsIncomplete, Expired,
    // CompactionToolLarge
    else
    {
        errorInfo = errorInfo + ", please try again!";
        ROCKSDB_LOG(WARNING) << LOG_DESC(errorInfo);
        BOOST_THROW_EXCEPTION(DatabaseNeedRetry() << errinfo_comment(errorInfo));
    }
}

Status BasicRocksDB::Write(WriteOptions const& options, WriteBatch& batch)
{
    auto status = m_db->Write(options, &batch);
    checkStatus(status);
    return status;
}

}  // namespace db
}  // namespace dev
