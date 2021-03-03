#include "object.hh"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

std::unique_ptr<QueryObject> QueryObject::map(
    const std::function<std::unique_ptr<QueryObject>(QueryObject *)> &mapper) {
    return mapper(this);
}

std::unique_ptr<QueryObject> QueryArray::map(
    const std::function<std::unique_ptr<QueryObject>(QueryObject *)> &mapper) {
    auto result = std::make_unique<QueryArray>();
    for (auto const &obj : list) {
        auto new_obj = mapper(obj.get());
        if (new_obj) {
            result->list.emplace_back(std::move(new_obj));
        }
    }
    return std::move(result);
}

void init_object(py::module &m) {
    auto obj = py::class_<QueryObject>(m, "QueryObject");
    obj.def("map", &QueryObject::map);

    auto array = py::class_<QueryArray>(m, "QueryArray");
}