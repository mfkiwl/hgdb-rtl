#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "../../src/rtl.hh"
#include "slang/binding/OperatorExpressions.h"
#include "slang/symbols/ASTVisitor.h"
#include "slang/symbols/InstanceSymbols.h"
#include "slang/symbols/PortSymbols.h"
#include "slang/symbols/VariableSymbols.h"

namespace py = pybind11;

struct QueryObject {
    hgdb::rtl::DesignDatabase *db = nullptr;
    virtual ~QueryObject() = default;
};

struct QueryArray : QueryObject {
    std::vector<QueryObject> list;
};

struct InstanceObject : public QueryObject {
    // this holds instance information
    const slang::InstanceSymbol *instance = nullptr;
};

struct VariableObject : public QueryObject {
    const slang::ValueSymbol *variable = nullptr;
};

struct PortObject : public VariableObject {
    const slang::PortSymbol *port = nullptr;
};

void init_instance_object(py::module &m) {
    auto cls = py::class_<InstanceObject, QueryObject>(m, "Instance");
    // we don't allow users to construct it by themself
    cls.def_property_readonly(
           "name", [](const InstanceObject &obj) { return std::string(obj.instance->name); })
        .def_property_readonly("path",
                               [](const InstanceObject &obj) {
                                   std::string name;
                                   obj.instance->getHierarchicalPath(name);
                                   return name;
                               })
        .def("__eq__",
             [](const InstanceObject &obj, const QueryObject &other) {
                 auto const *o = dynamic_cast<const InstanceObject *>(&other);
                 if (o) {
                     return o->instance == obj.instance;
                 }
                 return false;
             })
        .def("__hash__",
             [](const InstanceObject &obj) {
                 return
                     // we use the instance addr as hash value, which should be consistent with the
                     // __eq__ implementation
                     (uint64_t)obj.instance;
             })
        .def_property_readonly("definition", [](const InstanceObject &obj) {
            return obj.instance->getDefinition().name;
        });
}

void init_variable_object(py::module &m) {
    auto cls = py::class_<VariableObject, QueryObject>(m, "Variable");
    auto get_parent = [](const VariableObject &obj) -> std::unique_ptr<InstanceObject> {
        auto const *inst = obj.db->get_parent_instance(obj.variable);
        if (!inst) return nullptr;
        auto ptr = std::make_unique<InstanceObject>();
        ptr->db = obj.db;
        ptr->instance = inst;
        return std::move(ptr);
    };
    cls.def_property_readonly("name", [](const VariableObject &obj) { return obj.variable->name; })
        .def_property_readonly("instance",
                               [=](const VariableObject &obj) -> std::unique_ptr<InstanceObject> {
                                   return get_parent(obj);
                               })
        .def_property_readonly("parent",
                               [=](const VariableObject &obj) -> std::unique_ptr<InstanceObject> {
                                   return get_parent(obj);
                               });
}

void init_query(py::module &m) {
    init_instance_object(m);
    init_variable_object(m);
}