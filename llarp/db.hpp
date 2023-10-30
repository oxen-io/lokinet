#include <memory>

#include "util/fs.hpp"
#include "router_contact.hpp"

namespace llarp {

class DatabaseImpl;

/// Database class that manages an sqlite database for persistent storage of things like RCs.  This
/// database is initialized during startup (to load initial values) but after that is intended to
/// only be accessed from within the oxenmq DB worker thread to modify the stored data.
///
/// The data stored here is *only* read on startup, to restore state: once startup is complete, only
/// modifications are made to keep the stored database synced with the internal lokinet state.
class Database {
    std::unique_ptr<DatabaseImpl> impl;

    public:

    /// Constructs the database.
    explicit Database(const fs::path& db_path);

    /// Adds an RC to the database.  If an RC with the given pubkey already exists, this replaces it
    /// with the given one.
    void add_rc(const RouterContact& rc);

    /// Removes an RC from the database.  Returns true if found (and removed), false if not found.
    bool remove_rc(const RouterContact& rc);
};

};
