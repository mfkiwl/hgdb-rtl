#include "query.hh"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <regex>

namespace py = pybind11;

std::shared_ptr<QueryObject> Filter::apply(const std::shared_ptr<QueryObject> &data) {
    // first need to see if data is an array or not
    auto result = std::make_shared<QueryArray>(data->ooze);

    if (data->is_array()) {
        auto array = std::reinterpret_pointer_cast<QueryArray>(data);
        uint64_t size = array->size();
        for (uint64_t i = 0; i < size; i++) {
            auto obj = array->get(i);
            if (!obj) continue;
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

class RegexString {
public:
    explicit RegexString(const std::string &regex) { re_ = std::regex(regex); }

    bool equal(const std::string &input) {
        std::smatch match;
        return std::regex_match(input, match, re_);
    }

private:
    std::regex re_;
};

void init_query_helper_function(py::module &m) {
    py::class_<RegexString, std::shared_ptr<RegexString>>(m, "RegexString")
        .def("__eq__", [](const std::shared_ptr<RegexString> &re, const std::string &value) {
            return re->equal(value);
        });
    m.def("like",
          [](const std::string &pattern) { return std::make_shared<RegexString>(pattern); });
}