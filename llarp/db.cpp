#include "db.hpp"

#include <SQLiteCpp/SQLiteCpp.h>
#include "sqlite3.h"
#include "oxen/log/catlogger.hpp"

#include <shared_mutex>

namespace llarp {

constexpr std::chrono::milliseconds SQLite_busy_timeout = 10s;

namespace {
    auto logcat = log::Cat("db");

    template <typename T>
    constexpr bool is_cstr = false;
    template <size_t N>
    constexpr bool is_cstr<char[N]> = true;
    template <size_t N>
    constexpr bool is_cstr<const char[N]> = true;
    template <>
    [[maybe_unused]] inline constexpr bool is_cstr<char*> = true;
    template <>
    [[maybe_unused]] inline constexpr bool is_cstr<const char*> = true;

    // Simple wrapper class that can be used to bind a blob through the templated binding code
    // below. E.g. `exec_query(st, 100, 42, blob_binder{data})` binds the third parameter using
    // no-copy blob binding of the contained data.
    struct blob_binder {
        std::string_view data;
        explicit blob_binder(std::string_view d) : data{d} {}
        template <typename Char, typename = std::enable_if_t<sizeof(Char) == 1>>
        explicit blob_binder(const std::basic_string_view<Char>& d) :
                data{reinterpret_cast<const char*>(d.data()), d.size()} {}
    };

    // Binds a string_view as a no-copy blob at parameter index i.
    void bind_blob_ref(SQLite::Statement& st, int i, std::string_view blob) {
        st.bindNoCopy(i, static_cast<const void*>(blob.data()), static_cast<int>(blob.size()));
    }

    // Called from exec_query and similar to bind statement parameters for immediate execution.
    // strings (and c strings) use no-copy binding; integer values are bound by value.  You can bind
    // a blob (by reference, like strings) by passing `blob_binder{data}`.
    template <typename T>
    void bind_oneshot(SQLite::Statement& st, int& i, const T& val) {
        if constexpr (std::is_same_v<T, std::string> || is_cstr<T>)
            st.bindNoCopy(i++, val);
        else if constexpr (std::is_same_v<T, blob_binder>)
            bind_blob_ref(st, i++, val.data);
        else
            st.bind(i++, val);
    }

    // Executes a query that does not expect results.  Optionally binds parameters, if provided.
    // Returns the number of affected rows; throws on error or if results are returned.
    template <typename... T>
    int exec_query(SQLite::Statement& st, const T&... bind) {
        [[maybe_unused]] int i = 1;
        (bind_oneshot(st, i, bind), ...);
        return st.exec();
    }

    // Same as above, but prepares a literal query on the fly for use with queries that are only
    // used once.
    template <typename... T>
    int exec_query(SQLite::Database& db, const char* query, const T&... bind) {
        SQLite::Statement st{db, query};
        return exec_query(st, bind...);
    }

    template <typename T, typename... More>
    struct first_type {
        using type = T;
    };
    template <typename... T>
    using first_type_t = typename first_type<T...>::type;

    template <typename... T>
    struct tuple_or_pair_impl {
        using type = std::tuple<T...>;
    };
    template <typename T1, typename T2>
    struct tuple_or_pair_impl<T1, T2> {
        using type = std::pair<T1, T2>;
    };

    // Converts a parameter pack T... into either a plain T (if singleton), a pair (if exactly 2),
    // or a tuple<T...>:
    template <typename... T>
    using type_or_tuple = std::conditional_t<
            sizeof...(T) == 1,
            first_type_t<T...>,
            typename tuple_or_pair_impl<T...>::type>;

    // Retrieves a single row of values from the current state of a statement (i.e. after a
    // executeStep() call that is expecting a return value).  If `T...` is a single type then this
    // returns the single T value; if T... is two values you get back a pair, otherwise you get back
    // a tuple of values.
    template <typename... T>
    type_or_tuple<T...> get(SQLite::Statement& st) {
        using TT = type_or_tuple<T...>;
        if constexpr (sizeof...(T) == 1)
            return static_cast<TT>(st.getColumn(0));
        else
            return st.getColumns<TT, sizeof...(T)>();
    }

    // Steps a statement to completion that is expected to return at most one row, optionally
    // binding values into it (if provided).  Returns a filled out optional<T> (or
    // optional<std::tuple<T...>>) if a row was retrieved, otherwise a nullopt.  Throws if more
    // than one row is retrieved.
    template <typename... T, typename... Args>
    std::optional<type_or_tuple<T...>> exec_and_maybe_get(
            SQLite::Statement& st, const Args&... bind) {
        [[maybe_unused]] int i = 1;
        (bind_oneshot(st, i, bind), ...);
        std::optional<type_or_tuple<T...>> result;
        while (st.executeStep()) {
            if (result) {
                log::error(
                        logcat,
                        "Expected single-row result, got multiple rows from {}",
                        st.getQuery());
                throw std::runtime_error{"DB error: expected single-row result, got multiple rows"};
            }
            result = get<T...>(st);
        }
        return result;
    }

    // Executes a statement to completion that is expected to return exactly one row, optionally
    // binding values into it (if provided).  Returns a T or std::tuple<T...> (depending on
    // whether or not more than one T is provided) for the row.  Throws an exception if no rows
    // or more than one row are returned.
    template <typename... T, typename... Args>
    type_or_tuple<T...> exec_and_get(SQLite::Statement& st, const Args&... bind) {
        auto maybe_result = exec_and_maybe_get<T...>(st, bind...);
        if (!maybe_result) {
            log::error(logcat, "Expected single-row result, got no rows from {}", st.getQuery());
            throw std::runtime_error{"DB error: expected single-row result, got not rows"};
        }
        return *std::move(maybe_result);
    }

    // Executes a query to completion, collecting each row into a vector<T>, vector<pair<T1,T2>, or
    // vector<tuple<T...>>, depending on whether 1, 2, or more Ts are given.  Can optionally bind
    // before executing.
    template <typename... T, typename... Bind>
    std::vector<type_or_tuple<T...>> get_all(SQLite::Statement& st, const Bind&... bind) {
        [[maybe_unused]] int i = 1;
        (bind_oneshot(st, i, bind), ...);
        std::vector<type_or_tuple<T...>> results;
        while (st.executeStep())
            results.push_back(get<T...>(st));
        return results;
    }

    // Similar to get_all<K, V>, but returns a std::map<K, V> rather than a std::vector<pair<K, V>>.
    template <typename K, typename V, typename... Bind>
    std::map<K, V> get_map(SQLite::Statement& st, const Bind&... bind) {
        [[maybe_unused]] int i = 1;
        (bind_oneshot(st, i, bind), ...);
        std::map<K, V> results;
        while (st.executeStep())
            results[static_cast<K>(st.getColumn(0))] = static_cast<V>(st.getColumn(1));
        return results;
    }

}  // namespace

class DatabaseImpl {
    public:
        Database& parent;
        SQLite::Database db;

    // SQLiteCpp's statements are not thread-safe, so we prepare them thread-locally when needed
    std::unordered_map<std::thread::id, std::unordered_map<std::string, SQLite::Statement>>
            prepared_sts;
    std::shared_mutex prepared_sts_mutex;

    DatabaseImpl(Database& parent, const std::filesystem::path& db_path) :
            parent{parent},
            db{db_path / std::filesystem::u8path("storage.db"),
               SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE | SQLite::OPEN_FULLMUTEX,
               SQLite_busy_timeout.count()} {
        // Don't fail on these because we can still work even if they fail
        if (int rc = db.tryExec("PRAGMA journal_mode = WAL"); rc != SQLITE_OK)
            log::error(logcat, "Failed to set journal mode to WAL: {}", sqlite3_errstr(rc));

        if (int rc = db.tryExec("PRAGMA synchronous = NORMAL"); rc != SQLITE_OK)
            log::error(logcat, "Failed to set synchronous mode to NORMAL: {}", sqlite3_errstr(rc));

        if (int rc = db.tryExec("PRAGMA foreign_keys = ON"); rc != SQLITE_OK) {
            auto m = fmt::format(
                    "Failed to enable foreign keys constraints: {}", sqlite3_errstr(rc));
            log::critical(logcat, m);
            throw std::runtime_error{m};
        }
        int fk_enabled = db.execAndGet("PRAGMA foreign_keys").getInt();
        if (fk_enabled != 1) {
            log::critical(
                    logcat,
                    "Failed to enable foreign key constraints; perhaps this sqlite3 is "
                    "compiled without it?");
            throw std::runtime_error{"Foreign key support is required"};
        }

        if (!db.tableExists("router_contacts")) {
            create_schema();
        }

        /*
        if (!db.tableExists("some_new_table")) {
            log::info(logcat, "Upgrading database schema: adding FIXME");
            db.exec("CREATE TABLE ...");
        }
        */

        log::info(logcat, "Database setup complete");
    }

    void create_schema() {
        SQLite::Transaction transaction{db};

        db.exec(R"(
CREATE TABLE rcs (
    pubkey BLOB NOT NULL PRIMARY KEY,
    timestamp INTEGER NOT NULL,
    serialized BLOB NOT NULL,
    cooldown_until INTEGER, 
);
        )");

        if (db.tableExists("Data")) {
            log::warning(logcat, "Old database schema detected; performing migration...");

            // Migratation from old table structure:
            //
            // CREATE TABLE Data(
            //    Hash VARCHAR(128) NOT NULL,
            //    Owner VARCHAR(256) NOT NULL,
            //    TTL INTEGER NOT NULL,
            //    Timestamp INTEGER NOT NULL,
            //    TimeExpires INTEGER NOT NULL,
            //    Nonce VARCHAR(128) NOT NULL,
            //    Data BLOB
            // );

            SQLite::Statement ins_owner{
                    db, "INSERT INTO owners (type, pubkey) VALUES (?, ?) RETURNING id"};

            std::unordered_map<std::string, int> owner_ids;
            SQLite::Statement old_owners{db, "SELECT DISTINCT Owner FROM Data"};
            while (old_owners.executeStep()) {
                int type;
                std::array<char, 32> pubkey;
                std::string old_owner = old_owners.getColumn(0);
                if (old_owner.size() == 66 && util::starts_with(old_owner, "05") &&
                    oxenc::is_hex(old_owner)) {
                    type = 5;
                    oxenc::from_hex(old_owner.begin() + 2, old_owner.end(), pubkey.begin());
                } else if (old_owner.size() == 64 && oxenc::is_hex(old_owner)) {
                    type = 0;
                    oxenc::from_hex(old_owner.begin(), old_owner.end(), pubkey.begin());
                } else {
                    log::warning(
                            logcat, "Found invalid owner pubkey '{}' during migration; ignoring");
                    continue;
                }

                int id = exec_and_get<int>(ins_owner, type, old_owner);
                ins_owner.reset();
                owner_ids.emplace(std::move(old_owner), id);
            }

            log::warning(
                    logcat, "Migrated {} owner pubkeys.  Migrating messages...", owner_ids.size());

            SQLite::Statement ins_msg{
                    db,
                    "INSERT INTO messages (hash, owner, timestamp, expiry, "
                    "data) VALUES (?, ?, ?, ?, ?)"};

            SQLite::Statement sel_msgs{
                    db,
                    "SELECT Hash, Owner, Timestamp, TimeExpires, Data FROM Data ORDER BY rowid"};
            int msgs = 0, bad_owners = 0;
            while (sel_msgs.executeStep()) {
                auto [hash, owner, ts, exp, data] =
                        get<const char*, const char*, int64_t, int64_t, std::string>(sel_msgs);
                auto it = owner_ids.find(owner);
                if (it == owner_ids.end()) {
                    bad_owners++;
                    continue;
                }
                exec_query(ins_msg, hash, it->second, ts, exp, data);
                ins_msg.reset();
                msgs++;
            }

            log::warning(
                    logcat,
                    "Migrated {} messages ({} invalid owner ids); dropping old Data table",
                    msgs,
                    bad_owners);

            db.exec("DROP TABLE Data");

            log::warning(logcat, "Data migration complete!");
        }

        transaction.commit();
    }

    void views_triggers_indices() {
        // We create these separate from the table because it makes upgrading easier (we can just
        // drop the indices/views that we want to recreate).

        SQLite::Transaction transaction{db};

        db.exec(R"(
CREATE TRIGGER IF NOT EXISTS owner_autoclean
    AFTER DELETE ON messages FOR EACH ROW WHEN NOT EXISTS (SELECT * FROM messages WHERE owner = old.owner)
    BEGIN
        DELETE FROM owners WHERE id = old.owner;
    END;

CREATE INDEX IF NOT EXISTS messages_expiry ON messages(expiry);
CREATE INDEX IF NOT EXISTS messages_owner ON messages(owner, namespace, timestamp);
CREATE INDEX IF NOT EXISTS messages_hash ON messages(hash);

CREATE VIEW IF NOT EXISTS owned_messages AS
    SELECT owners.id AS oid, type, pubkey, messages.id AS mid, hash, namespace, timestamp, expiry, data
    FROM messages JOIN owners ON messages.owner = owners.id;

CREATE TRIGGER IF NOT EXISTS owned_messages_insert
    INSTEAD OF INSERT ON owned_messages FOR EACH ROW WHEN NEW.oid IS NULL
    BEGIN
        INSERT INTO owners (type, pubkey) VALUES (NEW.type, NEW.pubkey) ON CONFLICT DO NOTHING;
        INSERT INTO messages (id, hash, owner, namespace, timestamp, expiry, data) VALUES (
            NEW.mid,
            NEW.hash,
            (SELECT id FROM owners WHERE type = NEW.type AND pubkey = NEW.pubkey),
            NEW.namespace,
            NEW.timestamp,
            NEW.expiry,
            NEW.data
        );
    END;
        )");

        transaction.commit();
    }

    /** Wrapper around a SQLite::Statement that calls `tryReset()` on destruction of the
     * wrapper. */
    class StatementWrapper {
        SQLite::Statement& st;

      public:
        /// Whether we should reset on destruction; can be set to false if needed.
        bool reset_on_destruction = true;

        explicit StatementWrapper(SQLite::Statement& st) noexcept : st{st} {}
        ~StatementWrapper() noexcept {
            if (reset_on_destruction)
                st.tryReset();
        }
        SQLite::Statement& operator*() noexcept { return st; }
        SQLite::Statement* operator->() noexcept { return &st; }
        operator SQLite::Statement&() noexcept { return st; }
    };

    StatementWrapper prepared_st(const std::string& query) {
        std::unordered_map<std::string, SQLite::Statement>* sts;
        {
            std::shared_lock rlock{prepared_sts_mutex};
            if (auto it = prepared_sts.find(std::this_thread::get_id()); it != prepared_sts.end())
                sts = &it->second;
            else {
                rlock.unlock();
                std::unique_lock wlock{prepared_sts_mutex};
                sts = &prepared_sts.try_emplace(std::this_thread::get_id()).first->second;
            }
        }
        if (auto qit = sts->find(query); qit != sts->end())
            return StatementWrapper{qit->second};
        return StatementWrapper{sts->try_emplace(query, db, query).first->second};
    }

    template <typename... T>
    int prepared_exec(const std::string& query, const T&... bind) {
        return exec_query(prepared_st(query), bind...);
    }

    template <typename... T, typename... Bind>
    auto prepared_get(const std::string& query, const Bind&... bind) {
        return exec_and_get<T...>(prepared_st(query), bind...);
    }

    user_pubkey load_pubkey(uint8_t type, std::string pk) { return {type, std::move(pk)}; }
};

Database::Database(const std::filesystem::path& db_path) :
        impl{std::make_unique<DatabaseImpl>(*this, db_path)} {
    clean_expired();
}

Database::~Database() = default;

void Database::clean_expired() {
    impl->prepared_exec(
            "DELETE FROM messages WHERE expiry <= ?",
            to_epoch_ms(std::chrono::system_clock::now()));
}

int64_t Database::get_message_count() {
    return impl->prepared_get<int64_t>("SELECT COUNT(*) FROM messages");
}

int64_t Database::get_owner_count() {
    return impl->prepared_get<int64_t>("SELECT COUNT(*) FROM owners");
}

int64_t Database::get_used_bytes() {
    return impl->prepared_get<int64_t>("PRAGMA page_count") * impl->page_size;
}

static std::optional<message> get_message(DatabaseImpl& impl, SQLite::Statement& st) {
    std::optional<message> msg;
    while (st.executeStep()) {
        assert(!msg);
        auto [hash, otype, opubkey, ns, ts, exp, data] =
                get<std::string, uint8_t, std::string, namespace_id, int64_t, int64_t, std::string>(
                        st);
        msg.emplace(
                impl.load_pubkey(otype, std::move(opubkey)),
                std::move(hash),
                ns,
                from_epoch_ms(ts),
                from_epoch_ms(exp),
                std::move(data));
    }
    return msg;
}

std::optional<message> Database::retrieve_random() {
    clean_expired();
    auto st = impl->prepared_st(
            "SELECT hash, type, pubkey, namespace, timestamp, expiry, data"
            " FROM owned_messages "
            " WHERE mid = (SELECT id FROM messages ORDER BY RANDOM() LIMIT 1)");
    return get_message(*impl, st);
}

std::optional<message> Database::retrieve_by_hash(const std::string& msg_hash) {
    auto st = impl->prepared_st(
            "SELECT hash, type, pubkey, namespace, timestamp, expiry, data"
            " FROM owned_messages WHERE hash = ?");
    st->bindNoCopy(1, msg_hash);
    return get_message(*impl, st);
}

std::optional<bool> Database::store(const message& msg) {
    auto st = impl->prepared_st(
            "INSERT INTO owned_messages (pubkey, type, hash, namespace, timestamp, expiry, data)"
            " VALUES (?, ?, ?, ?, ?, ?, ?)");

    try {
        exec_query(
                st,
                msg.pubkey,
                msg.hash,
                msg.msg_namespace,
                to_epoch_ms(msg.timestamp),
                to_epoch_ms(msg.expiry),
                blob_binder{msg.data});
    } catch (const SQLite::Exception& e) {
        if (int rc = e.getErrorCode(); rc == SQLITE_CONSTRAINT)
            return false;
        else if (rc == SQLITE_FULL) {
            if (impl->db_full_counter++ % DB_FULL_FREQUENCY == 0)
                log::error(logcat, "Failed to store message: database is full");
            return std::nullopt;
        } else {
            log::error(logcat, "Failed to store message: {}", e.getErrorStr());
            throw;
        }
    }
    return true;
}

void Database::bulk_store(const std::vector<message>& items) {
    SQLite::Transaction t{impl->db};
    auto get_owner = impl->prepared_st("SELECT id FROM owners WHERE pubkey = ? AND type = ?");
    auto insert_owner = impl->prepared_st(
            "INSERT INTO owners (pubkey, type) VALUES (?, ?) ON CONFLICT DO NOTHING RETURNING id");
    std::unordered_map<user_pubkey, int64_t> seen;
    for (auto& m : items) {
        if (!m.pubkey)
            continue;
        if (auto [it, ins] = seen.emplace(m.pubkey, 0); ins) {
            auto ownerid = exec_and_maybe_get<int64_t>(get_owner, m.pubkey);
            get_owner->reset();
            if (!ownerid) {
                ownerid = exec_and_maybe_get<int64_t>(insert_owner, m.pubkey);
                insert_owner->reset();
            }
            if (ownerid)
                it->second = *ownerid;
            else {
                log::error(
                        logcat,
                        "Failed to insert owner {} for bulk store",
                        m.pubkey.prefixed_hex());
                seen.erase(it);
            }
        }
    }

    auto insert_message = impl->prepared_st(
            "INSERT INTO messages (owner, hash, namespace, timestamp, expiry, data)"
            " VALUES (?, ?, ?, ?, ?, ?)"
            " ON CONFLICT DO NOTHING");

    for (auto& m : items) {
        if (!m.pubkey)
            continue;
        auto owner_it = seen.find(m.pubkey);
        if (owner_it == seen.end())
            continue;

        exec_query(
                insert_message,
                owner_it->second,
                m.hash,
                m.msg_namespace,
                to_epoch_ms(m.timestamp),
                to_epoch_ms(m.expiry),
                blob_binder{m.data});
        insert_message->reset();
    }

    t.commit();
}

std::pair<std::vector<message>, bool> Database::retrieve(
        const user_pubkey& pubkey,
        namespace_id ns,
        const std::string& last_hash,
        std::optional<size_t> max_results,
        std::optional<size_t> max_size,
        const bool size_b64,
        const size_t per_message_overhead) {

    auto owner_st = impl->prepared_st("SELECT id FROM owners WHERE pubkey = ? AND type = ?");
    auto ownerid = exec_and_maybe_get<int64_t>(owner_st, pubkey);
    if (!ownerid)
        return {};

    if (max_results && *max_results < 1)
        max_results = 1;

    std::optional<int64_t> last_id;
    if (!last_hash.empty()) {
        auto st = impl->prepared_st(
                "SELECT id FROM messages WHERE owner = ? AND namespace = ? AND hash = ?");
        last_id = exec_and_maybe_get<int64_t>(st, *ownerid, to_int(ns), last_hash);
    }

    auto st = impl->prepared_st(
            last_id ? "SELECT hash, namespace, timestamp, expiry, data FROM messages "
                      "WHERE owner = ? AND namespace = ? AND id > ? ORDER BY id LIMIT ?"
                    : "SELECT hash, namespace, timestamp, expiry, data FROM messages "
                      "WHERE owner = ? AND namespace = ? ORDER BY id LIMIT ?");
    int pos = 1;
    st->bind(pos++, *ownerid);
    st->bind(pos++, to_int(ns));
    if (last_id)
        st->bind(pos++, *last_id);
    st->bind(pos++, max_results ? static_cast<int>(*max_results) + 1 : -1);

    std::pair<std::vector<message>, bool> result{};
    auto& [results, more] = result;

    size_t agg_size = 0;
    while (st->executeStep()) {
        auto [hash, ns, ts, exp, data] =
                get<std::string, namespace_id, int64_t, int64_t, std::string>(st);
        if (max_results && results.size() >= *max_results) {
            more = true;
            break;
        }
        if (max_size) {
            agg_size += per_message_overhead;
            agg_size += hash.size();
            agg_size += size_b64 ? data.size() * 4 / 3 : data.size();
            if (!results.empty() && agg_size > *max_size) {
                more = true;
                break;
            }
        }

        results.emplace_back(
                std::move(hash), ns, from_epoch_ms(ts), from_epoch_ms(exp), std::move(data));
    }

    return result;
}

std::vector<message> Database::retrieve_all() {
    std::vector<message> results;
    auto st = impl->prepared_st(
            "SELECT type, pubkey, hash, namespace, timestamp, expiry, data"
            " FROM owned_messages ORDER BY mid");

    while (st->executeStep()) {
        auto [type, pubkey, hash, ns, ts, exp, data] =
                get<uint8_t, std::string, std::string, namespace_id, int64_t, int64_t, std::string>(
                        st);
        results.emplace_back(
                impl->load_pubkey(type, pubkey),
                std::move(hash),
                ns,
                from_epoch_ms(ts),
                from_epoch_ms(exp),
                std::move(data));
    }

    return results;
}

std::vector<std::pair<namespace_id, std::string>> Database::delete_all(const user_pubkey& pubkey) {
    auto st = impl->prepared_st(
            "DELETE FROM messages"
            " WHERE owner = (SELECT id FROM owners WHERE pubkey = ? AND type = ?)"
            " RETURNING namespace, hash");
    return get_all<namespace_id, std::string>(st, pubkey);
}

std::vector<std::string> Database::delete_all(const user_pubkey& pubkey, namespace_id ns) {
    auto st = impl->prepared_st(
            "DELETE FROM messages"
            " WHERE owner = (SELECT id FROM owners WHERE pubkey = ? AND type = ?)"
            " AND namespace = ?"
            " RETURNING hash");
    return get_all<std::string>(st, pubkey, ns);
}

namespace {
    std::string multi_in_query(std::string_view prefix, size_t count, std::string_view suffix) {
        std::string query;
        query.reserve(prefix.size() + (count == 0 ? 0 : 2 * count - 1) + suffix.size());
        query += prefix;
        for (size_t i = 0; i < count; i++) {
            if (i > 0)
                query += ',';
            query += '?';
        }
        query += suffix;
        return query;
    }
}  // namespace

std::vector<std::string> Database::delete_by_hash(
        const user_pubkey& pubkey, const std::vector<std::string>& msg_hashes) {
    if (msg_hashes.size() == 1) {
        // Use an optimized prepared statement for very common single-hash deletions
        auto st = impl->prepared_st(
                "DELETE FROM messages"
                " WHERE owner = (SELECT id FROM owners WHERE pubkey = ? AND type = ?)"
                " AND hash = ?"
                " RETURNING hash");
        return get_all<std::string>(st, pubkey, msg_hashes[0]);
    }

    SQLite::Statement st{
            impl->db,
            multi_in_query(
                    "DELETE FROM messages"
                    " WHERE owner = (SELECT id FROM owners WHERE pubkey = ? AND type = ?)"
                    " AND hash IN ("sv,  // ?,?,?,...,?
                    msg_hashes.size(),
                    ") RETURNING hash"sv)};

    bind_pubkey(st, 1, 2, pubkey);
    for (size_t i = 0; i < msg_hashes.size(); i++)
        st.bindNoCopy(3 + i, msg_hashes[i]);
    return get_all<std::string>(st);
}

std::vector<std::pair<namespace_id, std::string>> Database::delete_by_timestamp(
        const user_pubkey& pubkey, std::chrono::system_clock::time_point timestamp) {
    auto st = impl->prepared_st(
            "DELETE FROM messages"
            " WHERE owner = (SELECT id FROM owners WHERE pubkey = ? AND type = ?)"
            " AND timestamp <= ?"
            " RETURNING hash");
    return get_all<namespace_id, std::string>(st, pubkey, to_epoch_ms(timestamp));
}

std::vector<std::string> Database::delete_by_timestamp(
        const user_pubkey& pubkey,
        namespace_id ns,
        std::chrono::system_clock::time_point timestamp) {
    auto st = impl->prepared_st(
            "DELETE FROM messages"
            " WHERE owner = (SELECT id FROM owners WHERE pubkey = ? AND type = ?)"
            " AND timestamp <= ? AND namespace = ?"
            " RETURNING hash");
    return get_all<std::string>(st, pubkey, to_epoch_ms(timestamp), ns);
}

void Database::revoke_subaccount(const user_pubkey& pubkey, const subaccount_token& subaccount) {
    auto insert_token = impl->prepared_st(
            "INSERT INTO revoked_subaccounts (owner, token) "
            "VALUES ((SELECT id FROM owners WHERE pubkey = ? AND type = ?), ?) "
            "ON CONFLICT DO NOTHING");
    exec_query(insert_token, pubkey, blob_binder{subaccount.view()});
}

bool Database::subaccount_revoked(const user_pubkey& pubkey, const subaccount_token& subaccount) {
    auto count = exec_and_get<int64_t>(
            impl->prepared_st("SELECT COUNT(*) FROM revoked_subaccounts WHERE token = ? AND "
                              "owner = (SELECT id FROM owners WHERE pubkey = ? AND type = ?)"),
            blob_binder{subaccount.view()},
            pubkey);
    return count > 0;
}

std::vector<std::string> Database::update_expiry(
        const user_pubkey& pubkey,
        const std::vector<std::string>& msg_hashes,
        std::chrono::system_clock::time_point new_exp,
        bool extend_only,
        bool shorten_only) {
    auto new_exp_ms = to_epoch_ms(new_exp);

    auto expiry_constraint = extend_only  ? " AND expiry < ?1"s
                           : shorten_only ? " AND expiry > ?1"s
                                          : ""s;
    if (msg_hashes.size() == 1) {
        // Pre-prepared version for the common single hash case
        auto st = impl->prepared_st(
                "UPDATE messages SET expiry = ? WHERE hash = ?"s + expiry_constraint +
                " AND owner = (SELECT id FROM owners WHERE pubkey = ? AND type = ?)"
                " RETURNING hash");
        return get_all<std::string>(st, new_exp_ms, msg_hashes[0], pubkey);
    }

    SQLite::Statement st{
            impl->db,
            multi_in_query(
                    "UPDATE messages SET expiry = ?"
                    " WHERE owner = (SELECT id FROM owners WHERE pubkey = ? AND type = ?)"s +
                            expiry_constraint + " AND hash IN (",  // ?,?,?,...,?
                    msg_hashes.size(),
                    ") RETURNING hash"sv)};
    st.bind(1, new_exp_ms);
    bind_pubkey(st, 2, 3, pubkey);
    for (size_t i = 0; i < msg_hashes.size(); i++)
        st.bindNoCopy(4 + i, msg_hashes[i]);

    return get_all<std::string>(st);
}

std::map<std::string, int64_t> Database::get_expiries(
        const user_pubkey& pubkey, const std::vector<std::string>& msg_hashes) {
    if (msg_hashes.size() == 1) {
        // Pre-prepared version for the common single hash case
        auto st = impl->prepared_st(
                "SELECT hash, expiry FROM messages WHERE hash = ?"
                " AND owner = (SELECT id FROM owners WHERE pubkey = ? AND type = ?)");
        return get_map<std::string, int64_t>(st);
    }

    SQLite::Statement st{
            impl->db,
            multi_in_query(
                    "SELECT hash, expiry FROM messages"
                    " WHERE owner = (SELECT id FROM owners WHERE pubkey = ? AND type = ?)"
                    " AND hash IN ("sv,  // ?,?,?,...,?
                    msg_hashes.size(),
                    ")"sv)};
    bind_pubkey(st, 1, 2, pubkey);
    for (size_t i = 0; i < msg_hashes.size(); i++)
        st.bindNoCopy(3 + i, msg_hashes[i]);

    return get_map<std::string, int64_t>(st);
}

std::vector<std::pair<namespace_id, std::string>> Database::update_all_expiries(
        const user_pubkey& pubkey, std::chrono::system_clock::time_point new_exp) {
    auto new_exp_ms = to_epoch_ms(new_exp);
    auto st = impl->prepared_st(
            "UPDATE messages SET expiry = ?"
            " WHERE expiry > ? AND owner = (SELECT id FROM owners WHERE pubkey = ? AND type = ?)"
            " RETURNING namespace, hash");
    return get_all<namespace_id, std::string>(st, new_exp_ms, new_exp_ms, pubkey);
}

std::vector<std::string> Database::update_all_expiries(
        const user_pubkey& pubkey, namespace_id ns, std::chrono::system_clock::time_point new_exp) {
    auto new_exp_ms = to_epoch_ms(new_exp);
    auto st = impl->prepared_st(
            "UPDATE messages SET expiry = ?"
            " WHERE expiry > ? AND owner = (SELECT id FROM owners WHERE pubkey = ? AND type = ?)"
            " AND namespace = ?"
            " RETURNING hash");
    return get_all<std::string>(st, new_exp_ms, new_exp_ms, pubkey, ns);
}

}  // namespace oxen
