#ifndef HGDB_RTL_RTL_HH
#define HGDB_RTL_RTL_HH

#include <set>
#include <unordered_map>

#include "slang/compilation/Compilation.h"
#include "slang/symbols/PortSymbols.h"

namespace hgdb::rtl {

class DesignDatabase {
public:
    explicit DesignDatabase(slang::Compilation &compilation);
    const slang::Symbol *select(const std::string &path);
    const slang::InstanceSymbol *get_instance(const std::string &path);
    static std::set<const slang::ValueSymbol *> get_connected_symbols(
        const slang::InstanceSymbol *instance, const slang::PortSymbol *port);
    static std::set<const slang::ValueSymbol *> get_connected_symbols(
        const slang::InstanceSymbol *instance, const std::string &port_name);
    std::set<const slang::ValueSymbol *> get_connected_symbols(const std::string &path,
                                                               const std::string &port_name);
    static std::string get_instance_definition_name(const slang::InstanceSymbol *symbol);
    static std::string get_instance_path(const slang::InstanceSymbol *symbol);
    static bool instance_inside(const slang::InstanceSymbol *child,
                                const slang::InstanceSymbol *parent);
    std::set<const slang::InstanceSymbol *> get_source_instances(
        const slang::InstanceSymbol *instance);
    std::set<const slang::InstanceSymbol *> get_sink_instances(
        const slang::InstanceSymbol *instance);
    const slang::InstanceSymbol *get_parent_instance(const slang::Symbol* symbol);

private:
    slang::Compilation &compilation_;
    std::unordered_map<std::string, const slang::InstanceSymbol *> instances_;
    std::unordered_map<const slang::InstanceSymbol *, const slang::InstanceSymbol *>
        hierarchy_map_;

    void index_instance();
    std::set<const slang::InstanceSymbol *> get_connected_instance(
        const slang::InstanceSymbol *target_instance, const slang::PortSymbol *port,
        slang::ArgumentDirection direction);
    const slang::InstanceSymbol *get_instance_from_scope(const slang::Scope *scope);
};

}  // namespace hgdb::rtl

#endif  // HGDB_RTL_RTL_HH
