#include "rtl.hh"

#include <iostream>
#include <set>
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
    explicit SymbolExprVisitor(std::set<const slang::ValueSymbol *> &symbols) : symbols_(symbols) {}

    template <typename T>
    void visit(const T &symbol) {
        if constexpr (std::is_base_of<slang::ValueSymbol, T>::value) {
            symbols_.emplace(&symbol);
        } else if constexpr (std::is_base_of<slang::Expression, T>::value) {
            if constexpr (std::is_same<slang::Expression, T>::value) {
                symbol.template visit(*this);
            } else if constexpr (std::is_same<slang::BinaryExpression, T>::value ||
                                 std::is_same<slang::ConditionalExpression, T>::value) {
                visit(symbol.left());
                visit(symbol.right());
            } else if constexpr (std::is_same<slang::NamedValueExpression, T>::value) {
                symbols_.emplace(&symbol.symbol);
            } else if constexpr (std::is_same<slang::UnaryExpression, T>::value) {
                visit(symbol.operand());
            } else if constexpr (std::is_same<slang::ConcatenationExpression, T>::value) {
                for (auto const *s : symbol.operands()) {
                    visit(s);
                }
            } else if constexpr (std::is_same<slang::AssignmentExpression, T>::value) {
                visit(symbol.left());
            }
        }
    }

    void visitExpr(const slang::Expression &expr) { expr.visit(*this); }

    template <typename T>
    [[maybe_unused]] void visitInvalid(const T &) {}

private:
    std::set<const slang::ValueSymbol *> &symbols_;
};

std::set<const slang::ValueSymbol *> DesignDatabase::get_connected_symbols(
    const slang::InstanceSymbol *instance, const slang::PortSymbol *port) {
    auto const *conn = instance->getPortConnection(*port);
    if (conn) {
        auto const *other = conn->expr;
        std::set<const slang::ValueSymbol *> symbols;
        SymbolExprVisitor visitor(symbols);
        visitor.visitExpr(*other);
        return symbols;
    }
    return {};
}

std::set<const slang::ValueSymbol *> DesignDatabase::get_connected_symbols(
    const slang::InstanceSymbol *instance, const std::string &port_name) {
    auto const *p = instance->body.findPort(port_name);
    if (!p) return {};
    if (!slang::PortSymbol::isKind(p->kind)) return {};
    auto const &port = p->as<slang::PortSymbol>();
    return get_connected_symbols(instance, &port);
}

std::set<const slang::ValueSymbol *> DesignDatabase::get_connected_symbols(
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

bool DesignDatabase::instance_inside(const slang::InstanceSymbol *child,
                                     const slang::InstanceSymbol *parent) {
    // there are two approaches. the easiest way is just to compare their hierarchy path
    if (!child || !parent) return false;
    auto child_inst_name = get_instance_path(child);
    auto parent_inst_name = get_instance_path(parent);
    auto pos = child_inst_name.find(parent_inst_name);
    if (pos != std::string::npos) {
        // has to have a dot afterwards
        if (child_inst_name.length() > parent_inst_name.length()) {
            return child_inst_name[parent_inst_name.length()] == '.';
        }
    }
    return false;
}

class InstanceConnectedVisitor {
public:
    explicit InstanceConnectedVisitor(std::set<const slang::InstanceSymbol *> &instances,
                                      const slang::ValueSymbol *target_symbol,
                                      slang::ArgumentDirection target_dir)
        : instances_(instances), target_symbol_(target_symbol), target_dir_(target_dir) {}

    void visit(const slang::InstanceSymbol *instance) {
        for (auto const &member : instance->body.members()) {
            if (slang::InstanceSymbol::isKind(member.kind)) {
                auto const *inst = &member.as<slang::InstanceSymbol>();
                visitInstance(inst);
            }
        }
    }

    void visitInstance(const slang::InstanceSymbol *instance) {
        // find out if they have any connections to targeted symbol
        auto const &port_list = instance->body.getPortList();
        for (auto const *port : port_list) {
            auto const &p = port->as<slang::PortSymbol>();
            if (p.direction != target_dir_) {
                continue;
            }
            // need to figure out if we need input or output
            // need to get the port connection
            auto const &conn = instance->getPortConnection(p);
            auto const &expr = conn->expr;
            std::set<const slang::ValueSymbol *> connected_symbols;
            SymbolExprVisitor visitor(connected_symbols);
            visitor.visitExpr(*expr);
            if (connected_symbols.find(target_symbol_) != connected_symbols.end()) {
                instances_.emplace(instance);
                return;
            }
        }
    }

private:
    std::set<const slang::InstanceSymbol *> &instances_;
    const slang::ValueSymbol *target_symbol_;
    slang::ArgumentDirection target_dir_;
};

std::set<const slang::InstanceSymbol *> DesignDatabase::get_connected_instance(
    const slang::InstanceSymbol *target_instance, const slang::PortSymbol *port,
    slang::ArgumentDirection direction) {
    std::set<const slang::InstanceSymbol *> result;
    // if direction doesn't match
    if (port->direction != direction) return result;
    // this is an input port
    auto port_symbols = get_connected_symbols(target_instance, port);
    // notice that we need to get the instance symbols
    for (auto const *s : port_symbols) {
        // depends on the type of the symbol, we need to compute different things
        // if it is a variable, then we see if it's used to connected to another
        // instance
        // if it's a port, then it has to be connected through outside
        if (slang::VariableSymbol::isKind(s->kind)) {
            // test if it's a port or not
            if (target_instance->body.findPort(s->name)) {
                // legal SystemVerilog only allows parent's port to be connected,
                // if the connection is a port
                if (hierarchy_map_.find(target_instance) == hierarchy_map_.end()) {
                    continue;
                }
                auto const *parent = hierarchy_map_.at(target_instance);
                if (&parent->body == &s->getParentScope()->asSymbol()) {
                    result.emplace(parent);
                }
            } else {
                auto const &var = s->as<slang::VariableSymbol>();
                // need to find if it's connected to any instances
                // we use a visitor to find that connection
                // before doing that we need to find parent instance
                if (hierarchy_map_.find(target_instance) == hierarchy_map_.end()) {
                    continue;
                }
                auto const *parent_instance = hierarchy_map_.at(target_instance);
                InstanceConnectedVisitor visitor(result, &var,
                                                 direction == slang::ArgumentDirection::In
                                                     ? slang::ArgumentDirection::Out
                                                     : slang::ArgumentDirection::In);
                visitor.visit(parent_instance);
            }
        }
    }

    return result;
}

std::set<const slang::InstanceSymbol *> DesignDatabase::get_source_instances(
    const slang::InstanceSymbol *instance) {
    // given the instance, we find the source instances that connect to the target instance
    std::set<const slang::InstanceSymbol *> result;
    auto const &ports = instance->body.getPortList();
    for (auto const &port_s : ports) {
        if (slang::PortSymbol::isKind(port_s->kind)) {
            auto const &p = port_s->as<slang::PortSymbol>();
            auto instances = get_connected_instance(instance, &p, slang::ArgumentDirection::In);
            for (auto const *inst : instances) {
                result.emplace(inst);
            }
        }
    }

    return result;
}

std::set<const slang::InstanceSymbol *> DesignDatabase::get_sink_instances(
    const slang::InstanceSymbol *instance) {
    // given the instance, we find the source instances that connect to the target instance
    std::set<const slang::InstanceSymbol *> result;
    auto const &ports = instance->body.getPortList();
    for (auto const &port_s : ports) {
        if (slang::PortSymbol::isKind(port_s->kind)) {
            auto const &p = port_s->as<slang::PortSymbol>();
            auto instances = get_connected_instance(instance, &p, slang::ArgumentDirection::Out);
            for (auto const *inst : instances) {
                result.emplace(inst);
            }
        }
    }

    return result;
}

class InstanceVisitor {
public:
    explicit InstanceVisitor(
        std::unordered_map<std::string, const slang::InstanceSymbol *> &instances,
        std::unordered_map<const slang::InstanceSymbol *, const slang::InstanceSymbol *>
            &hierarchy_map)
        : instances_(instances), hierarchy_map_(hierarchy_map), hierarchy_() {}

    template <typename T>
    void visit(const T &elem) {
        if constexpr (std::is_base_of<slang::Symbol, T>::value) {
            if (slang::InstanceSymbol::isKind(elem.kind)) {
                auto const &instance = elem.template as<slang::InstanceSymbol>();
                // setup register hierarchy as well
                if (!hierarchy_.empty()) {
                    auto const *parent = hierarchy_.top();
                    hierarchy_map_.emplace(&instance, parent);
                }

                auto const &body = instance.body;
                std::string instance_name;
                instance.getHierarchicalPath(instance_name);
                instances_.emplace(instance_name, &instance);
                // push to stack
                hierarchy_.emplace(&instance);

                visit(instance.body);

                // pop the stack
                hierarchy_.pop();

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
    std::unordered_map<const slang::InstanceSymbol *, const slang::InstanceSymbol *>
        &hierarchy_map_;
    std::stack<const slang::InstanceSymbol *> hierarchy_;
};

void DesignDatabase::index_instance() {
    InstanceVisitor visitor(instances_, hierarchy_map_);
    visitor.visit(compilation_.getRoot());
}

const slang::InstanceSymbol *DesignDatabase::get_instance_from_scope(const slang::Scope *scope) {
    // if performance becomes an issue, turn this into a lookup table
    if (!scope) return nullptr;
    auto const &symbol = scope->asSymbol();
    if (slang::InstanceBodySymbol::isKind(symbol.kind)) {
        for (auto const &instance : instances_) {
            if (&instance.second->body == &symbol) {
                return instance.second;
            }
        }
    }
    return nullptr;
}

}  // namespace hgdb::rtl