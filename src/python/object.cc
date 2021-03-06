#include "object.hh"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <sstream>

#include "query.hh"

namespace py = pybind11;

std::shared_ptr<QueryObject> QueryObject::map(
    const std::function<std::shared_ptr<QueryObject>(QueryObject *)> &mapper) {
    return mapper(this);
}

std::shared_ptr<QueryObject> QueryArray::map(
    const std::function<std::shared_ptr<QueryObject>(QueryObject *)> &mapper) {
    auto result = std::make_shared<QueryArray>();
    auto len = size();
    for (uint64_t i = 0; i < len; i++) {
        auto obj = get(i);
        // cast it here so that it will register in python
        auto py_obj = py::cast(obj);
        (void)(py_obj);
        auto new_obj = mapper(obj.get());
        if (new_obj) {
            result->add(new_obj);
        }
    }
    return std::move(result);
}

void QueryArray::add(const std::shared_ptr<QueryObject> &obj) { data.emplace_back(obj); }

std::shared_ptr<QueryObject> flatten_size_one_array(const std::shared_ptr<QueryObject> &obj) {
    if (obj->is_array()) {
        auto array = std::reinterpret_pointer_cast<QueryArray>(obj);
        if (array->size() == 1) {
            return array->get(0);
        }
    }
    return obj;
}

std::shared_ptr<QueryObject> filter_query_object(
    const std::shared_ptr<QueryObject> &obj,
    const std::function<bool(const std::shared_ptr<QueryObject> &)> &func) {
    auto filter = Filter(func);
    auto r = filter.apply(obj);
    return flatten_size_one_array(r);
}

std::shared_ptr<QueryObject> filter_query_object_kwargs(const std::shared_ptr<QueryObject> &obj,
                                                        const py::kwargs &kwargs) {
    // need to construct the function
    auto func = [&](const std::shared_ptr<QueryObject> &target) -> bool {
        auto py_obj = py::cast(target);
        for (auto const &[name, value] : kwargs) {  // NOLINT
            if (!py::hasattr(py_obj, name)) return false;
            auto const &v = py_obj.attr(name);
            if (!value.equal(v)) return false;
        }
        return true;
    };

    auto filter = Filter(func);
    auto r = filter.apply(obj);
    return flatten_size_one_array(r);
}

std::shared_ptr<QueryObject> select_type(const std::shared_ptr<QueryObject> &obj,
                                         const py::object &type) {
    // if it's not an array it's easy
    if (!obj->is_array()) {
        auto py_obj = py::cast(obj);
        if (py_obj.get_type().is(type)) {
            return obj;
        } else {
            return nullptr;
        }
    }

    auto array = std::reinterpret_pointer_cast<QueryArray>(obj);
    auto result = std::make_shared<QueryArray>();
    for (auto const &entry : array->data) {
        auto py_obj = py::cast(entry);
        if (entry->is_array()) {
            // recursively calls itself
            auto r = select_type(entry, type);
            if (r->is_array()) {
                auto const &r_array = std::reinterpret_pointer_cast<QueryArray>(r);
                if (!r_array->empty()) {
                    result->add(r);
                }
            } else {
                result->add(r);
            }
        } else {
            if (py_obj.get_type().is(type)) {
                result->add(entry);
            }
        }
    }
    // we also flatten out one layer if it's nested size 1 loop
    return flatten_size_one_array(result);
}

std::shared_ptr<QueryObject> map_object(
    const std::shared_ptr<QueryObject> &obj,
    const std::function<std::shared_ptr<QueryObject>(const std::shared_ptr<QueryObject> &)> &func) {
    if (obj->is_array()) {
        auto const &array = std::reinterpret_pointer_cast<QueryArray>(obj);
        auto result = std::make_shared<QueryArray>();
        for (auto const &entry : array->data) {
            auto o = map_object(entry, func);
            if (o) {
                result->add(o);
            }
        }
        if (result->empty()) {
            return nullptr;
        } else if (result->size() == 1) {
            return result->get(0);
        } else {
            return result;
        }
    } else {
        auto o = func(obj);
        return o;
    }
}

void init_query_object(py::module &m) {
    auto obj = py::class_<QueryObject, std::shared_ptr<QueryObject>>(m, "QueryObject");
    obj.def("map", &QueryObject::map);
    obj.def("__repr__", [](const QueryObject &obj) {
        // pretty print the values
        // we will just use json to print out stuff
        py::dict dict;
        auto const values = obj.values();
        for (auto const &[name, value] : values) {
            dict[name.c_str()] = value;
        }
        return py::str(dict);
    });

    // the filter part
    obj.def(
        "filter",
        [](const std::shared_ptr<QueryObject> &obj,
           const std::function<bool(const std::shared_ptr<QueryObject> &)> &func) {
            return filter_query_object(obj, func);
        },
        py::arg("predicate"));
    obj.def("filter", &filter_query_object_kwargs);
    // notice that where is the same as filter
    obj.def(
        "where",
        [](const std::shared_ptr<QueryObject> &obj,
           const std::function<bool(const std::shared_ptr<QueryObject> &)> &func) {
            return filter_query_object(obj, func);
        },
        py::arg("predicate"));
    obj.def("where", &filter_query_object_kwargs);
    obj.def(
        "select",
        [](const std::shared_ptr<QueryObject> &obj, const std::string &attr) -> py::object {
            if (obj->is_array()) {
                // return as an array
                auto const &array = std::reinterpret_pointer_cast<QueryArray>(obj);
                py::list result;
                uint64_t size = array->size();
                for (uint64_t i = 0; i < size; i++) {
                    auto const &entry = array->get(i);
                    auto py_obj = py::cast(entry);
                    auto attr_value = py_obj.attr(attr.c_str());
                    result.append(attr_value);
                }
                return result;
            } else {
                // get the attributes
                auto py_obj = py::cast(obj);
                // let pybind throw exceptions
                auto attr_value = py_obj.attr(attr.c_str());
                return attr_value;
            }
        },
        py::arg("attr_name"), py::prepend());
    obj.def(
        "select",
        [](const std::shared_ptr<QueryObject> &obj, const py::object &type)
            -> std::shared_ptr<QueryObject> { return select_type(obj, type); },
        py::arg("type"));
    obj.def(
        "map",
        [](const std::shared_ptr<QueryObject> &obj,
           const std::function<std::shared_ptr<QueryObject>(const std::shared_ptr<QueryObject> &)>
               &func) { return map_object(obj, func); });
    obj.def("__eq__",
            [](const std::shared_ptr<QueryObject> &a, const std::shared_ptr<QueryObject> &b) {
                return py::str(py::cast(a)).equal(py::str(py::cast(b)));
            });

    obj.def("__hash__", [](const std::shared_ptr<QueryObject> &obj) {
        return py::hash(py::str(py::cast(obj)));
    });
}

void init_query_array(py::module &m) {
    auto array = py::class_<QueryArray, QueryObject, std::shared_ptr<QueryArray>>(m, "QueryArray");

    // implement array interface
    array.def("__len__", [](const QueryArray &array) { return array.size(); })
        .def(
            "__getitem__",
            [](const QueryArray &array, uint64_t index) {
                if (index >= array.size()) {
                    throw py::index_error();
                }
                return array.get(index);
            },
            py::return_value_policy::reference)
        .def("__iter__", [](QueryArray &array) { return py::make_iterator(array.data); });
    array.def("__repr__", [](const QueryArray &array) {
        py::list list;
        for (auto const &value : array.data) {
            auto obj = py::cast(value);
            list.append(obj);
        }
        return py::str(list);
    });
    array.def("map", &QueryArray::map, py::arg("predicate"));
}

void init_object(py::module &m) {
    init_query_object(m);
    init_query_array(m);
}
