#include "rtl.hh"

#include "slang/symbols/CompilationUnitSymbols.h"
#include "slang/symbols/PortSymbols.h"
#include "slang/symbols/InstanceSymbols.h"

namespace hgdb::rtl {

const slang::Symbol* DesignDatabase::select(const std::string& path) {
    auto const &root = compilation_.getRoot();
    return root.lookupName(path);
}

std::vector<const slang::Symbol*> DesignDatabase::get_connected_symbol(
    const std::string& path) {
    std::vector<const slang::Symbol*> result;
    auto const* symbol = select(path);
    // only if the symbol is a port
    if (symbol->kind == slang::SymbolKind::Port) {
        auto const &port = symbol->as<slang::PortSymbol>();
        auto const *parent = port.getParentScope();
        auto const &parent_symbol = parent->asSymbol();
        if (parent_symbol.kind == slang::SymbolKind::Instance) {
            auto const &instance = parent_symbol.as<slang::InstanceSymbol>();
            auto const *conn = instance.getPortConnection(port);
            if (conn) {
                auto const *other = conn->port;
                result.emplace_back(other);
            }
        }
    }
    return result;
}

}  // namespace hgdb::rtl