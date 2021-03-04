#include "object.hh"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <sstream>

#include "query.hh"
#include "rtl.hh"

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
        auto new_obj = mapper(obj.get());
        if (new_obj) {
            result->add(new_obj);
        }
    }
    return std::move(result);
}

void QueryArray::add(const std::shared_ptr<QueryObject> &obj) { data.emplace_back(obj); }

std::shared_ptr<QueryObject> filter_query_object(
    const std::shared_ptr<QueryObject> &obj, const std::function<bool(const std::shared_ptr<QueryObject> &)> &func) {
    auto filter = Filter(func);
    auto r = filter.apply(obj);
    return r;
}

void init_object(py::module &m) {
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
    // notice that where is the same as filter
    obj.def(
        "where",
        [](const std::shared_ptr<QueryObject> &obj,
           const std::function<bool(const std::shared_ptr<QueryObject> &)> &func) {
          return filter_query_object(obj, func);
        },
        py::arg("predicate"));

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
}
