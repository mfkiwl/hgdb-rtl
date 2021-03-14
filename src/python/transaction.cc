#include "transaction.hh"

#include "fmt/format.h"
#include "log.hh"

namespace py = pybind11;

Transaction::Transaction(uint64_t id, const std::shared_ptr<QueryArray> &items)
    : QueryArray(*items), id(id) {
    // make sure each item is log item
    for (auto const &item : data) {
        auto p = std::dynamic_pointer_cast<LogItem>(item);
        if (!p) {
            throw py::value_error(fmt::format("{0} is not a valid LogItem",
                                              py::str(py::cast(item)).cast<std::string>()));
        }
    }
}

uint64_t Transaction::duration() const {
    uint64_t min_ = std::numeric_limits<uint64_t>::max();
    uint64_t max_ = 0;
    for (auto const &item : data) {
        auto p = std::dynamic_pointer_cast<LogItem>(item);
        if (!p) {
            throw py::value_error(fmt::format("{0} is not a valid LogItem",
                                              py::str(py::cast(item)).cast<std::string>()));
        }
        auto i = p->get_item();
        if (min_ > i.time) {
            min_ = i.time;
        }
        if (max_ < i.time) {
            max_ = i.time;
        }
    }
    return max_ - min_;
}

void init_transaction(py::module &m) {
    auto t = py::class_<Transaction, QueryArray, std::shared_ptr<Transaction>>(m, "Transaction");
    t.def(py::init<uint64_t, const std::shared_ptr<QueryArray> &>());
    t.def_property_readonly("duration", &Transaction::duration);

    t.def_readwrite("id", &Transaction::id);
}
