#include "query.hh"
#include <pybind11/pybind11.h>

namespace py = pybind11;

std::shared_ptr<QueryObject> Filter::apply(const std::shared_ptr<QueryObject> &data) {
    // first need to see if data is an array or not
    auto result = std::make_shared<QueryArray>();

    if (data->is_array()) {
        auto array = std::reinterpret_pointer_cast<QueryArray>(data);
        uint64_t size = array->size();
        for (uint64_t i = 0; i < size; i++) {
            auto obj = array->get(i);
            if (!obj)
                continue;
            auto r = func_(obj);
            if (r) {
                result->add(obj);
            }
        }
    } else {
        auto r = func_(data);
        if (r) {
            return data;
        }
    }

    return result;
}