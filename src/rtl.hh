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
    static std::string get_symbol_path(const slang::Symbol *symbol);
    static bool symbol_inside(const slang::Symbol *child, const slang::InstanceSymbol *parent);
    std::set<const slang::InstanceSymbol *> get_source_instances(
        const slang::InstanceSymbol *instance);
    std::set<const slang::InstanceSymbol *> get_source_instances(const slang::PortSymbol *port);
    std::set<const slang::InstanceSymbol *> get_sink_instances(
        const slang::InstanceSymbol *instance);
    std::set<const slang::InstanceSymbol *> get_sink_instances(const slang::PortSymbol *port);
    const slang::InstanceSymbol *get_parent_instance(const slang::Symbol *symbol);
    static const slang::PortSymbol *get_port(const slang::Symbol *variable);

    const std::vector<const slang::VariableSymbol *> &variables() const { return variables_; }
    const std::vector<const slang::InstanceSymbol *> &instances() const { return instances_; }
    const std::vector<const slang::PortSymbol *> &ports() const { return ports_; }

private:
    slang::Compilation &compilation_;
    std::unordered_map<std::string, const slang::InstanceSymbol *> instances_map_;
    std::unordered_map<const slang::InstanceSymbol *, const slang::InstanceSymbol *> hierarchy_map_;
    std::vector<const slang::InstanceSymbol *> instances_;
    std::vector<const slang::PortSymbol *> ports_;
    std::vector<const slang::VariableSymbol *> variables_;

    void index_values();
    std::set<const slang::InstanceSymbol *> get_connected_instances(
        const slang::InstanceSymbol *target_instance, const slang::PortSymbol *port,
        slang::ArgumentDirection direction);
    const slang::InstanceSymbol *get_instance_from_scope(const slang::Scope *scope);

    std::set<const slang::InstanceSymbol *> get_connected_instances(const slang::PortSymbol *port);
};

}  // namespace hgdb::rtl

#endif  // HGDB_RTL_RTL_HH
