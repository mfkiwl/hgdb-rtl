#ifndef HGDB_RTL_RTL_HH
#define HGDB_RTL_RTL_HH

#include <unordered_map>

#include "slang/compilation/Compilation.h"
#include "slang/symbols/PortSymbols.h"

namespace hgdb::rtl {

class DesignDatabase {
public:
    explicit DesignDatabase(slang::Compilation &compilation);
    const slang::Symbol *select(const std::string &path);
    const slang::InstanceSymbol *get_instance(const std::string &path);
    static std::vector<const slang::ValueSymbol *> get_connected_symbols(
        const slang::InstanceSymbol *instance, const slang::PortSymbol *port);
    static std::vector<const slang::ValueSymbol *> get_connected_symbols(
        const slang::InstanceSymbol *instance, const std::string &port_name);
    std::vector<const slang::ValueSymbol *> get_connected_symbols(const std::string &path,
                                                             const std::string &port_name);
    static std::string get_instance_definition_name(const slang::InstanceSymbol* symbol);
    static std::string get_instance_path(const slang::InstanceSymbol *symbol);
    static bool instance_inside(const slang::InstanceSymbol* child, const slang::InstanceSymbol *parent);

private:
    slang::Compilation &compilation_;
    std::unordered_map<std::string, const slang::InstanceSymbol *> instances_;

    void index_instance();
};

}  // namespace hgdb::rtl

#endif  // HGDB_RTL_RTL_HH
