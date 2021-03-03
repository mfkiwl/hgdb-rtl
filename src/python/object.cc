#include "object.hh"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "fmt/format.h"

namespace py = pybind11;

template <typename T>
std::unique_ptr<InstanceObject> get_parent_instance(const T &obj) {
    const slang::InstanceSymbol *inst = nullptr;
    if constexpr (std::is_base_of<slang::ValueSymbol, T>::value) {
        inst = obj.db->get_parent_instance(obj.variable);

    } else if constexpr (std::is_same<slang::InstanceSymbol, T>::value) {
        inst = obj.db->get_parent_instance(obj.instance);
    }

    if (!inst) return nullptr;
    auto ptr = std::make_unique<InstanceObject>(obj.db, inst);
    return std::move(ptr);
}

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
        .def_property_readonly(
            "definition",
            [](const InstanceObject &obj) { return obj.instance->getDefinition().name; })
        .def_property_readonly("parent",
                               [](const InstanceObject &obj) { return get_parent_instance(obj); });
}

void init_variable_object(py::module &m) {
    auto cls = py::class_<VariableObject, QueryObject>(m, "Variable");
    cls.def_property_readonly("name", [](const VariableObject &obj) { return obj.variable->name; })
        .def_property_readonly("instance",
                               [](const VariableObject &obj) -> std::unique_ptr<InstanceObject> {
                                   return get_parent_instance(obj);
                               })
        .def_property_readonly("parent",
                               [](const VariableObject &obj) -> std::unique_ptr<InstanceObject> {
                                   return get_parent_instance(obj);
                               });
}

void init_port_object(py::module &m) {
    auto cls = py::class_<PortObject, VariableObject>(m, "Port");
    cls.def_property_readonly("direction", [](const PortObject &port) -> std::string {
        switch (port.port->direction) {
            case (slang::ArgumentDirection::In):
                return "in";
            case (slang::ArgumentDirection::Out):
                return "out";
            case (slang::ArgumentDirection::InOut):
                return "inout";
            case (slang::ArgumentDirection::Ref):
                return "ref";
        }
        throw std::runtime_error("Unknown port direction");
    });
}

void init_query_array(py::module &m) {
    auto cls = py::class_<QueryArray, QueryObject>(m, "QueryArray");
    // implement array interface
    cls.def("__len__", [](const QueryArray &array) { return array.list.size(); })
        .def("__getitem__", [](const QueryArray &array, uint64_t index) {
            if (index >= array.list.size()) {
                throw std::out_of_range(
                    fmt::format("Array size: {0}. Index: {1}", array.list.size(), index));
            }
            return array.list[index];
        });
}

void init_object(py::module &m) {
    init_instance_object(m);
    init_variable_object(m);
    init_port_object(m);
    init_query_array(m);
}