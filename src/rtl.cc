#include "rtl.hh"

#include <iostream>
#include <stack>

#include "fmt/format.h"
#include "slang/binding/OperatorExpressions.h"
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
    explicit SymbolExprVisitor(std::vector<const slang::ValueSymbol *> &symbols) : symbols_(symbols) {}

    template <typename T>
    void visit(const T &symbol) {
        if constexpr (std::is_base_of<slang::ValueSymbol, T>::value) {
            symbols_.emplace_back(&symbol);
        } else if constexpr (std::is_base_of<slang::Expression, T>::value) {
            if constexpr (std::is_same<slang::Expression, T>::value) {
                symbol.template visit(*this);
            } else if constexpr (std::is_same<slang::BinaryExpression, T>::value ||
                                 std::is_same<slang::ConditionalExpression, T>::value) {
                visit(symbol.left());
                visit(symbol.right());
            } else if constexpr (std::is_same<slang::NamedValueExpression, T>::value) {
                symbols_.emplace_back(&symbol.symbol);
            } else if constexpr (std::is_same<slang::UnaryExpression, T>::value) {
                visit(symbol.operand());
            } else if constexpr (std::is_same<slang::ConcatenationExpression, T>::value) {
                for (auto const *s : symbol.operands()) {
                    visit(s);
                }
            }
        }
    }

    template <typename T>
    [[maybe_unused]] void visitInvalid(const T &) {}

private:
    std::vector<const slang::ValueSymbol *> &symbols_;
};

std::vector<const slang::ValueSymbol *> DesignDatabase::get_connected_symbols(
    const slang::InstanceSymbol *instance, const slang::PortSymbol *port) {
    auto const *conn = instance->getPortConnection(*port);
    if (conn) {
        auto const *other = conn->expr;
        std::vector<const slang::ValueSymbol *> symbols;
        SymbolExprVisitor visitor(symbols);
        visitor.visit(*other);
        return symbols;
    }
    return {};
}

std::vector<const slang::ValueSymbol *> DesignDatabase::get_connected_symbols(
    const slang::InstanceSymbol *instance, const std::string &port_name) {
    auto const *p = instance->body.findPort(port_name);
    if (!p) return {};
    if (!slang::PortSymbol::isKind(p->kind)) return {};
    auto const &port = p->as<slang::PortSymbol>();
    return get_connected_symbols(instance, &port);
}

std::vector<const slang::ValueSymbol *> DesignDatabase::get_connected_symbols(
    const std::string &path, const std::string &port_name) {
    if (instances_.find(path) == instances_.end()) return {};
    auto const *inst = instances_.at(path);
    return get_connected_symbols(inst, port_name);
}

std::string DesignDatabase::get_instance_definition_name(const slang::InstanceSymbol *symbol) {
    return std::string(symbol->getDefinition().name);
}

std::string DesignDatabase::get_instance_path(const slang::InstanceSymbol *symbol) {
    std::string result;
    symbol->getHierarchicalPath(result);
    return result;
}

bool DesignDatabase::instance_inside(const slang::InstanceSymbol *child, const slang::InstanceSymbol *parent) {
    // there are two approaches, we could just use hierarchy path to figure out if they overlap
    // completely. but for performance reason we do simple looping
    const slang::Scope *scope = child->getParentScope();
    while (scope != nullptr) {
        auto const *scope_symbol = &scope->asSymbol();
        if (scope_symbol == &parent->body) {
            return true;
        }
        scope = scope_symbol->getParentScope();
    }
    return false;
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
                auto const &name = instance.name;
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