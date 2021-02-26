#ifndef HGDB_RTL_RTL_HH
#define HGDB_RTL_RTL_HH

#include "slang/compilation/Compilation.h"

namespace hgdb::rtl {

class DesignDatabase {
public:
    explicit DesignDatabase(slang::Compilation &compilation): compilation_(compilation) {}
    const slang::Symbol* select(const std::string &path);
    std::vector<const slang::Symbol*> get_connected_symbol(const std::string &path);

private:
    slang::Compilation &compilation_;
};

}

#endif  // HGDB_RTL_RTL_HH
