#include "object.hh"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

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

std::vector<QueryObject *> QueryArray::list() {
    std::vector<QueryObject *> result;
    result.reserve(list_.size());
    for (auto const &obj : list_) result.emplace_back(obj.get());
    return result;
}

void QueryArray::add(const std::shared_ptr<QueryObject> &obj) { list_.emplace_back(obj); }

void init_object(py::module &m) {
    auto obj = py::class_<QueryObject, std::shared_ptr<QueryObject>>(m, "QueryObject");
    obj.def("map", &QueryObject::map);

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
            py::return_value_policy::reference);
}

// type caster for objects
namespace pybind11 {
template <>
struct [[maybe_unused]] polymorphic_type_hook<QueryObject> {
    static const void *get(const QueryObject *src, const std::type_info *&type) {
        if (src) {
            auto const &info = src->type_info();
            type = &info;
            return src;
        }
        return src;
    }
};
}  // namespace pybind11