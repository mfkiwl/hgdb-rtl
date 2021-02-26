#include "rtl.hh"

#include <iostream>
#include <stack>

#include "fmt/format.h"
#include "slang/symbols/ASTVisitor.h"
#include "slang/symbols/InstanceSymbols.h"
#include "slang/symbols/PortSymbols.h"
#include "slang/symbols/VariableSymbols.h"

namespace hgdb::rtl {

DesignDatabase::DesignDatabase(slang::Compilation &compilation) : compilation_(compilation) {
    index_instance();
}

const slang::Symbol *DesignDatabase::select(const std::string &path) {
    // if we've already computed the instance, use it
    if (instances_.find(path) != instances_.end()) {
        return instances_.at(path);
    } else {
        auto const &root = compilation_.getRoot();
        return root.lookupName(path);
    }
}

const slang::InstanceSymbol *DesignDatabase::get_instance(const std::string &path) {
    if (instances_.find(path) != instances_.end()) {
        return instances_.at(path);
    } else {
        return nullptr;
    }
}

// visitor that collect all symbols
class SymbolExprVisitor {
public:
    explicit SymbolExprVisitor(std::vector<const slang::Symbol *> &symbols) : symbols_(symbols) {}

    template <typename T>
    void visit(const T &elem) {
        if constexpr (std::is_base_of<slang::Symbol, T>::value) {
            symbols_.emplace_back(&elem);
        } else if constexpr (std::is_base_of<slang::Expression, T>::value) {
            elem.visit(*this);
        }
    }
    void visitInvalid(const slang::Expression &) {}

private:
    std::vector<const slang::Symbol *> &symbols_;
};

std::vector<const slang::Symbol *> DesignDatabase::get_connected_symbols(
    const slang::InstanceSymbol *instance, const slang::PortSymbol *port) {
    auto const *conn = instance->getPortConnection(*port);
    if (conn) {
        auto const *other = conn->expr;
        std::vector<const slang::Symbol *> symbols;
        SymbolExprVisitor visitor(symbols);
        visitor.visit(*other->getSymbolReference());
        return symbols;
    }
    return {};
}

class InstanceVisitor {
public:
    explicit InstanceVisitor(
        std::unordered_map<std::string, const slang::InstanceSymbol *> &instances)
        : instances_(instances), hierarchy_() {}

    template <typename T>
    void visit(const T &elem) {
        if constexpr (std::is_base_of<slang::Symbol, T>::value) {
            if (slang::InstanceSymbol::isKind(elem.kind)) {
                auto const &instance = elem.template as<slang::InstanceSymbol>();

                auto const &body = instance.body;
                auto const &name = body.name;
                auto instance_name = get_hierarchy_name(instance.name);
                instances_.emplace(instance_name, &instance);
                // push to stack
                hierarchy_.emplace_back(std::string(name));

                visit(instance.body);

                // pop the stack
                hierarchy_.pop_back();
                return;
            }
        }
        if constexpr (std::is_base_of<slang::Scope, T>::value) {
            auto const &scope = elem.template as<slang::Scope>();
            for (auto const &mem : scope.members()) {
                visit(mem);
            }
        }
    }

private:
    std::unordered_map<std::string, const slang::InstanceSymbol *> &instances_;
    std::vector<std::string> hierarchy_;

    std::string get_hierarchy_name(const std::string_view &sv) {
        std::string result;
        if (hierarchy_.empty()) {
            result = std::string(sv);
        } else {
            auto parent = fmt::format("{0}", fmt::join(hierarchy_.begin(), hierarchy_.end(), "."));
            return fmt::format("{0}.{1}", parent, sv);
        }

        return result;
    }
};

void DesignDatabase::index_instance() {
    InstanceVisitor visitor(instances_);
    visitor.visit(compilation_.getRoot());
}

}  // namespace hgdb::rtl