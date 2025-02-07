/*
    This file is part of cpp-ethereum.

    cpp-ethereum is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cpp-ethereum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file State.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "State.h"

#include "Defaults.h"
#include <libconfig/GlobalConfigure.h>
#include <libdevcore/Assertions.h>
#include <libdevcore/LevelDB.h>
#include <libdevcore/TrieHash.h>
#include <libdevcore/easylog.h>
#include <libsecurity/EncryptedLevelDB.h>
#include <boost/filesystem.hpp>
#include <boost/timer.hpp>

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::mptstate;
namespace fs = boost::filesystem;
namespace devdb = dev::db;

State::State(u256 const& _accountStartNonce, OverlayDB const& _db, BaseState _bs)
  : m_db(_db), m_state(&m_db), m_accountStartNonce(_accountStartNonce)
{
    if (_bs != BaseState::PreExisting)
        // Initialise to the state entailed by the genesis block; this guarantees the trie is built
        // correctly.
        m_state.init();
}

State::State(State const& _s)
  : m_db(_s.m_db),
    m_state(&m_db, _s.m_state.root(), Verification::Skip),
    m_cache(_s.m_cache),
    m_unchangedCacheEntries(_s.m_unchangedCacheEntries),
    m_nonExistingAccountsCache(_s.m_nonExistingAccountsCache),
    m_touched(_s.m_touched),
    m_accountStartNonce(_s.m_accountStartNonce),
    m_changeLog(_s.m_changeLog)
{}

OverlayDB State::openDB(fs::path const& _basePath, h256 const&, WithExisting _we)
{
    fs::path path = _basePath.empty() ? Defaults::get()->m_dbPath : _basePath;
    path /= fs::path("state");

    if (_we == WithExisting::Kill)
    {
        LOG(DEBUG) << "statedb: Killing state database (WithExisting::Kill).";
        fs::remove_all(path);
    }
    fs::create_directories(path);
    DEV_IGNORE_EXCEPTIONS(fs::permissions(path, fs::owner_all));

    try
    {
        db::BasicLevelDB* basicDB = NULL;
        leveldb::Status status;

        if (g_BCOSConfig.diskEncryption.enable)
            status = devdb::EncryptedLevelDB::Open(devdb::LevelDB::defaultDBOptions(),
                path.string(), &basicDB, g_BCOSConfig.diskEncryption.cipherDataKey);
        else
            status = devdb::BasicLevelDB::Open(
                devdb::LevelDB::defaultDBOptions(), path.string(), &basicDB);

        devdb::LevelDB::checkStatus(status, path);

        std::shared_ptr<db::DatabaseFace> dbFace = std::make_shared<devdb::LevelDB>(basicDB);

        LOG(TRACE) << "statedb Opened state DB.";
        return OverlayDB(dbFace);
    }
    catch (boost::exception const& ex)
    {
        LOG(WARNING) << boost::diagnostic_information(ex);
        if (fs::space(path).available < 1024)
        {
            LOG(WARNING) << "Not enough available space found on hard drive. Please free some up "
                            "and then re-run. Bailing.";
            BOOST_THROW_EXCEPTION(NotEnoughAvailableSpace());
        }
        else
        {
            LOG(WARNING) << "Database " << (path)
                         << "already open. You appear to have another instance of ethereum "
                            "running. Bailing.";
            BOOST_THROW_EXCEPTION(DatabaseAlreadyOpen());
        }
    }
}

void State::populateFrom(AccountMap const& _map)
{
    mptstate::commit(_map, m_state);
    commit();
}

u256 const& State::requireAccountStartNonce() const
{
    if (m_accountStartNonce == Invalid256)
        BOOST_THROW_EXCEPTION(InvalidAccountStartNonceInState());
    return m_accountStartNonce;
}

void State::noteAccountStartNonce(u256 const& _actual)
{
    if (m_accountStartNonce == Invalid256)
        m_accountStartNonce = _actual;
    else if (m_accountStartNonce != _actual)
        BOOST_THROW_EXCEPTION(IncorrectAccountStartNonceInState());
}

void State::removeEmptyAccounts()
{
    for (auto& i : m_cache)
        if (i.second.isDirty() && i.second.isEmpty())
            i.second.kill();
}

State& State::operator=(State const& _s)
{
    if (&_s == this)
        return *this;

    m_db = _s.m_db;
    m_state.open(&m_db, _s.m_state.root(), Verification::Skip);
    m_cache = _s.m_cache;
    m_unchangedCacheEntries = _s.m_unchangedCacheEntries;
    m_nonExistingAccountsCache = _s.m_nonExistingAccountsCache;
    m_touched = _s.m_touched;
    m_accountStartNonce = _s.m_accountStartNonce;
    return *this;
}

Account const* State::account(Address const& _a) const
{
    return const_cast<State*>(this)->account(_a);
}

Account* State::account(Address const& _addr)
{
    auto it = m_cache.find(_addr);
    if (it != m_cache.end())
        return &it->second;

    if (m_nonExistingAccountsCache.find(_addr) != m_nonExistingAccountsCache.end())
        return nullptr;

    // Populate basic info.
    string stateBack = m_state.at(_addr);
    if (stateBack.empty())
    {
        m_nonExistingAccountsCache.insert(_addr);
        return nullptr;
    }

    clearCacheIfTooLarge();

    RLP state(stateBack);
    auto account = Account(state[0].toInt<u256>(), state[1].toInt<u256>(), state[2].toHash<h256>(),
        state[3].toHash<h256>(), Account::Unchanged);
    auto i = m_cache.insert(std::make_pair(_addr, account));
#if 0
    auto i = m_cache.emplace(std::piecewise_construct, std::forward_as_tuple(_addr),
        std::forward_as_tuple(state[0].toInt<u256>(), state[1].toInt<u256>(),
            state[2].toHash<h256>(), state[3].toHash<h256>(), Account::Unchanged));
#endif
    m_unchangedCacheEntries.push_back(_addr);
    return &i.first->second;
}

void State::clearCacheIfTooLarge() const
{
    // TODO: Find a good magic number
    while (m_unchangedCacheEntries.size() > 1000)
    {
        // Remove a random element
        // FIXME: Do not use random device as the engine. The random device should be only used to
        // seed other engine.
        size_t const randomIndex = std::uniform_int_distribution<size_t>(
            0, m_unchangedCacheEntries.size() - 1)(dev::s_fixedHashEngine);

        Address const addr = m_unchangedCacheEntries[randomIndex];
        swap(m_unchangedCacheEntries[randomIndex], m_unchangedCacheEntries.back());
        m_unchangedCacheEntries.pop_back();

        auto cacheEntry = m_cache.find(addr);
        if (cacheEntry != m_cache.end() && !cacheEntry->second.isDirty())
            m_cache.erase(cacheEntry);
    }
}

void State::commit()
{
    // Remove empty accounts by default
    removeEmptyAccounts();
    m_touched += dev::mptstate::commit(m_cache, m_state);
    m_changeLog.clear();
    m_cache.clear();
    m_unchangedCacheEntries.clear();
}

unordered_map<Address, u256> State::addresses() const
{
#if ETH_FATDB
    unordered_map<Address, u256> ret;
    for (auto& i : m_cache)
        if (i.second.isAlive())
            ret[i.first] = i.second.balance();
    for (auto const& i : m_state)
        if (m_cache.find(i.first) == m_cache.end())
            ret[i.first] = RLP(i.second)[1].toInt<u256>();
    return ret;
#else
    BOOST_THROW_EXCEPTION(InterfaceNotSupported() << errinfo_interface("State::addresses()"));
#endif
}

std::pair<State::AddressMap, h256> State::addresses(
    h256 const& _beginHash, size_t _maxResults) const
{
    AddressMap addresses;
    h256 nextKey;

#if ETH_FATDB
    for (auto it = m_state.hashedLowerBound(_beginHash); it != m_state.hashedEnd(); ++it)
    {
        auto const address = Address(it.key());
        auto const itCachedAddress = m_cache.find(address);

        // skip if deleted in cache
        if (itCachedAddress != m_cache.end() && itCachedAddress->second.isDirty() &&
            !itCachedAddress->second.isAlive())
            continue;

        // break when _maxResults fetched
        if (addresses.size() == _maxResults)
        {
            nextKey = h256((*it).first);
            break;
        }

        h256 const hashedAddress((*it).first);
        addresses[hashedAddress] = address;
    }
#endif

    // get addresses from cache with hash >= _beginHash (both new and old touched, we can't
    // distinguish them) and order by hash
    AddressMap cacheAddresses;
    for (auto const& addressAndAccount : m_cache)
    {
        auto const& address = addressAndAccount.first;
        auto const addressHash = sha3(address);
        auto const& account = addressAndAccount.second;
        if (account.isDirty() && account.isAlive() && addressHash >= _beginHash)
            cacheAddresses.emplace(addressHash, address);
    }

    // merge addresses from DB and addresses from cache
    addresses.insert(cacheAddresses.begin(), cacheAddresses.end());

    // if some new accounts were created in cache we need to return fewer results
    if (addresses.size() > _maxResults)
    {
        auto itEnd = std::next(addresses.begin(), _maxResults);
        nextKey = itEnd->first;
        addresses.erase(itEnd, addresses.end());
    }

    return {addresses, nextKey};
}


void State::setRoot(h256 const& _r)
{
    m_cache.clear();
    m_unchangedCacheEntries.clear();
    m_nonExistingAccountsCache.clear();
    //  m_touched.clear();
    m_state.setRoot(_r);
}

bool State::addressInUse(Address const& _id) const
{
    return !!account(_id);
}

bool State::accountNonemptyAndExisting(Address const& _address) const
{
    if (Account const* a = account(_address))
        return !a->isEmpty();
    else
        return false;
}

bool State::addressHasCode(Address const& _id) const
{
    if (auto a = account(_id))
        return a->codeHash() != EmptySHA3;
    else
        return false;
}

u256 State::balance(Address const& _id) const
{
    if (auto a = account(_id))
        return a->balance();
    else
        return 0;
}

void State::incNonce(Address const& _addr)
{
    if (Account* a = account(_addr))
    {
        auto oldNonce = a->nonce();
        a->incNonce();
        m_changeLog.emplace_back(_addr, oldNonce);
    }
    else
        // This is possible if a transaction has gas price 0.
        createAccount(_addr, Account(requireAccountStartNonce() + 1, 0));
}

void State::setNonce(Address const& _addr, u256 const& _newNonce)
{
    if (Account* a = account(_addr))
    {
        auto oldNonce = a->nonce();
        a->setNonce(_newNonce);
        m_changeLog.emplace_back(_addr, oldNonce);
    }
    else
        // This is possible when a contract is being created.
        createAccount(_addr, Account(_newNonce, 0));
}

void State::addBalance(Address const& _id, u256 const& _amount)
{
    if (Account* a = account(_id))
    {
        // Log empty account being touched. Empty touched accounts are cleared
        // after the transaction, so this event must be also reverted.
        // We only log the first touch (not dirty yet), and only for empty
        // accounts, as other accounts does not matter.
        // TODO: to save space we can combine this event with Balance by having
        //       Balance and Balance+Touch events.
        if (!a->isDirty() && a->isEmpty())
            m_changeLog.emplace_back(Change::Touch, _id);

        // Increase the account balance. This also is done for value 0 to mark
        // the account as dirty. Dirty account are not removed from the cache
        // and are cleared if empty at the end of the transaction.
        a->addBalance(_amount);
    }
    else
        createAccount(_id, {requireAccountStartNonce(), _amount});

    if (_amount)
        m_changeLog.emplace_back(Change::Balance, _id, _amount);
}

void State::subBalance(Address const& _addr, u256 const& _value)
{
    if (_value == 0)
        return;

    Account* a = account(_addr);
    if (!a || a->balance() < _value)
        // TODO: I expect this never happens.
        BOOST_THROW_EXCEPTION(NotEnoughCash());

    // Fall back to addBalance().
    addBalance(_addr, 0 - _value);
}

void State::setBalance(Address const& _addr, u256 const& _value)
{
    Account* a = account(_addr);
    u256 original = a ? a->balance() : 0;

    // Fall back to addBalance().
    addBalance(_addr, _value - original);
}

void State::createContract(Address const& _address)
{
    createAccount(_address, {requireAccountStartNonce(), 0});
}

void State::createAccount(Address const& _address, Account const&& _account)
{
    // ensured by the program logic
    assert(!addressInUse(_address) && "Account already exists");

    m_cache[_address] = std::move(_account);
    m_nonExistingAccountsCache.erase(_address);
    m_changeLog.emplace_back(Change::Create, _address);
}

void State::kill(Address _addr)
{
    if (auto a = account(_addr))
        a->kill();
    // If the account is not in the db, nothing to kill.
}

u256 State::getNonce(Address const& _addr) const
{
    if (auto a = account(_addr))
        return a->nonce();
    else
        return m_accountStartNonce;
}

/// The hash of the root of our state tree.
h256 State::rootHash(bool) const
{
    return m_state.root();
}

/// modify here to enable account storage cache
u256 State::storage(Address const& _id, u256 const& _key)
{
    if (Account* a = account(_id))
    {
        auto mit = a->storageOverlay().find(_key);
        if (mit != a->storageOverlay().end())
            return mit->second;

        // Not in the storage cache - go to the DB.
        SecureTrieDB<h256, OverlayDB> memdb(const_cast<OverlayDB*>(&m_db),
            a->baseRoot());  // promise we won't change the overlay! :)
        string payload = memdb.at(_key);
        u256 ret = payload.size() ? RLP(payload).toInt<u256>() : 0;
        a->setStorage(_key, ret);
        return ret;
    }
    else
        return 0;
}

void State::setStorage(Address const& _contract, u256 const& _key, u256 const& _value)
{
    m_changeLog.emplace_back(_contract, _key, storage(_contract, _key));
    m_cache[_contract].setStorage(_key, _value);
}

void State::clearStorage(Address const& _contract)
{
    h256 const& oldHash{m_cache[_contract].baseRoot()};
    if (oldHash == EmptyTrie)
        return;
    m_changeLog.emplace_back(Change::StorageRoot, _contract, oldHash);
    m_cache[_contract].clearStorage();
}

map<h256, pair<u256, u256>> State::storage(Address const& _id) const
{
#if ETH_FATDB
    map<h256, pair<u256, u256>> ret;

    if (Account const* a = account(_id))
    {
        // Pull out all values from trie storage.
        if (h256 root = a->baseRoot())
        {
            SecureTrieDB<h256, OverlayDB> memdb(
                const_cast<OverlayDB*>(&m_db), root);  // promise we won't alter the overlay! :)

            for (auto it = memdb.hashedBegin(); it != memdb.hashedEnd(); ++it)
            {
                h256 const hashedKey((*it).first);
                u256 const key = h256(it.key());
                u256 const value = RLP((*it).second).toInt<u256>();
                ret[hashedKey] = make_pair(key, value);
            }
        }

        // Then merge cached storage over the top.
        for (auto const& i : a->storageOverlay())
        {
            h256 const key = i.first;
            h256 const hashedKey = sha3(key);
            if (i.second)
                ret[hashedKey] = i;
            else
                ret.erase(hashedKey);
        }
    }
    return ret;
#else
    (void)_id;
    BOOST_THROW_EXCEPTION(
        InterfaceNotSupported() << errinfo_interface("State::storage(Address const& _id)"));
#endif
}

h256 State::storageRoot(Address const& _id) const
{
    string s = m_state.at(_id);
    if (s.size())
    {
        RLP r(s);
        return r[2].toHash<h256>();
    }
    return EmptyTrie;
}

bytes const& State::code(Address const& _addr) const
{
    Account const* a = account(_addr);
    if (!a || a->codeHash() == EmptySHA3)
        return NullBytes;

    if (a->code().empty())
    {
        // Load the code from the backend.
        Account* mutableAccount = const_cast<Account*>(a);
        mutableAccount->noteCode(m_db.lookup(a->codeHash()));
        CodeSizeCache::instance().store(a->codeHash(), a->code().size());
    }

    return a->code();
}

void State::setCode(Address const& _address, bytes&& _code)
{
    m_changeLog.emplace_back(_address, code(_address));
    m_cache[_address].setCode(std::move(_code));
}

h256 State::codeHash(Address const& _a) const
{
    if (Account const* a = account(_a))
        return a->codeHash();
    else
        return EmptySHA3;
}

size_t State::codeSize(Address const& _a) const
{
    if (Account const* a = account(_a))
    {
        if (a->hasNewCode())
            return a->code().size();
        auto& codeSizeCache = CodeSizeCache::instance();
        h256 codeHash = a->codeHash();
        if (codeSizeCache.contains(codeHash))
            return codeSizeCache.get(codeHash);
        else
        {
            size_t size = code(_a).size();
            codeSizeCache.store(codeHash, size);
            return size;
        }
    }
    else
        return 0;
}

size_t State::savepoint() const
{
    return m_changeLog.size();
}

void State::rollback(size_t _savepoint)
{
    while (_savepoint != m_changeLog.size())
    {
        auto& change = m_changeLog.back();
        auto& account = m_cache[change.address];

        // Public State API cannot be used here because it will add another
        // change log entry.
        switch (change.kind)
        {
        case Change::Storage:
            account.setStorage(change.key, change.value);
            break;
        case Change::StorageRoot:
            account.setStorageRoot(change.value);
            break;
        case Change::Balance:
            account.addBalance(0 - change.value);
            break;
        case Change::Nonce:
            account.setNonce(change.value);
            break;
        case Change::Create:
            m_cache.erase(change.address);
            break;
        case Change::Code:
            account.setCode(std::move(change.oldCode));
            break;
        case Change::Touch:
            account.untouch();
            m_unchangedCacheEntries.emplace_back(change.address);
            break;
        }
        m_changeLog.pop_back();
    }
}
#if 0
std::ostream& dev::mptstate::operator<<(std::ostream& _out, State const& _s)
{
    _out << "--- " << _s.rootHash() << std::endl;
    std::set<Address> d;
    std::set<Address> dtr;
    auto trie = SecureTrieDB<Address, OverlayDB>(const_cast<OverlayDB*>(&_s.m_db), _s.rootHash());
    for (auto i : trie)
        d.insert(i.first), dtr.insert(i.first);
    for (auto i : _s.m_cache)
        d.insert(i.first);

    for (auto i : d)
    {
        auto it = _s.m_cache.find(i);
        Account* cache = it != _s.m_cache.end() ? &it->second : nullptr;
        string rlpString = dtr.count(i) ? trie.at(i) : "";
        RLP r(rlpString);
        assert(cache || r);

        if (cache && !cache->isAlive())
            _out << "XXX  " << i << std::endl;
        else
        {
            string lead = (cache ? r ? " *   " : " +   " : "     ");
            if (cache && r && cache->nonce() == r[0].toInt<u256>() &&
                cache->balance() == r[1].toInt<u256>())
                lead = " .   ";

            stringstream contout;

            if ((cache && cache->codeHash() == EmptySHA3) ||
                (!cache && r && (h256)r[3] != EmptySHA3))
            {
                std::map<u256, u256> mem;
                std::set<u256> back;
                std::set<u256> delta;
                std::set<u256> cached;
                if (r)
                {
                    SecureTrieDB<h256, OverlayDB> memdb(const_cast<OverlayDB*>(&_s.m_db),
                        r[2].toHash<h256>());  // promise we won't alter the overlay! :)
                    for (auto const& j : memdb)
                    {
                        auto it = mem.find(j.first);
                        auto v = RLP(j.second).toInt<u256>();
                        if (it != mem.end())
                        {
                            it->second = v;
                        }
                        else
                        {
                            mem.insert(std::make_pair(j.first, v));
                        }
                        // mem[j.first] = RLP(j.second).toInt<u256>();
                        back.insert(j.first);
                    }
                }
                if (cache)
                    for (auto const& j : cache->storageOverlay())
                    {
                        if ((!mem.count(j.first) && j.second) ||
                            (mem.count(j.first) && mem.at(j.first) != j.second))
                        {
                            auto it = mem.find(j.first);
                            if (it != mem.end())
                            {
                                it->second = j.second;
                            }
                            else
                            {
                                mem.insert(std::make_pair(j.first, j.second));
                            }

                            // mem[j.first] = j.second;
                            delta.insert(j.first);
                        }
                        else if (j.second)
                            cached.insert(j.first);
                    }
                if (!delta.empty())
                    lead = (lead == " .   ") ? "*.*  " : "***  ";

                contout << " @:";
                if (!delta.empty())
                    contout << "???";
                else
                    contout << r[2].toHash<h256>();
                if (cache && cache->hasNewCode())
                    contout << " $" << toHex(cache->code());
                else
                    contout << " $" << (cache ? cache->codeHash() : r[3].toHash<h256>());

                for (auto const& j : mem)
                    if (j.second)
                        contout << std::endl
                                << (delta.count(j.first) ?
                                           back.count(j.first) ? " *     " : " +     " :
                                           cached.count(j.first) ? " .     " : "       ")
                                << std::hex << nouppercase << std::setw(64) << j.first << ": "
                                << std::setw(0) << j.second;
                    else
                        contout << std::endl
                                << "XXX    " << std::hex << nouppercase << std::setw(64) << j.first
                                << "";
            }
            else
                contout << " [SIMPLE]";
            _out << lead << i << ": " << std::dec << (cache ? cache->nonce() : r[0].toInt<u256>())
                 << " #:" << (cache ? cache->balance() : r[1].toInt<u256>()) << contout.str()
                 << std::endl;
        }
    }
    return _out;
}

#endif

template <class DB>
AddressHash dev::mptstate::commit(AccountMap const& _cache, SecureTrieDB<Address, DB>& _state)
{
    AddressHash ret;
    for (auto const& i : _cache)
        if (i.second.isDirty())
        {
            if (!i.second.isAlive())
                _state.remove(i.first);
            else
            {
                RLPStream s(4);
                s << i.second.nonce() << i.second.balance();

                if (i.second.storageOverlay().empty())
                {
                    // for programming debug
                    assert(i.second.baseRoot());
                    s.append(i.second.baseRoot());
                }
                else
                {
                    SecureTrieDB<h256, DB> storageDB(_state.db(), i.second.baseRoot());
                    for (auto const& j : i.second.storageOverlay())
                        if (j.second)
                            storageDB.insert(j.first, rlp(j.second));
                        else
                            storageDB.remove(j.first);
                    // for programming debug
                    assert(storageDB.root());
                    s.append(storageDB.root());
                }

                if (i.second.hasNewCode())
                {
                    h256 ch = i.second.codeHash();
                    // Store the size of the code
                    CodeSizeCache::instance().store(ch, i.second.code().size());
                    _state.db()->insert(ch, &i.second.code());
                    s << ch;
                }
                else
                    s << i.second.codeHash();

                _state.insert(i.first, &s.out());
            }
            ret.insert(i.first);
        }
    return ret;
}


template AddressHash dev::mptstate::commit<OverlayDB>(
    AccountMap const& _cache, SecureTrieDB<Address, OverlayDB>& _state);
template AddressHash dev::mptstate::commit<MemoryDB>(
    AccountMap const& _cache, SecureTrieDB<Address, MemoryDB>& _state);
