#include "transaction.hh"

#include "fmt/format.h"
#include "log.hh"

namespace py = pybind11;

Transaction::Transaction(uint64_t id, const std::shared_ptr<QueryArray> &items)
    : QueryArray(*items), id(id) {
    // make sure each item is log item
    items_.reserve(data.size());
    for (auto const &item : data) {
        auto p = std::dynamic_pointer_cast<LogItem>(item);
        if (!p) {
            throw py::value_error(fmt::format("{0} is not a valid LogItem",
                                              py::str(py::cast(item)).cast<std::string>()));
        }
        items_.emplace_back(p.get());
    }
}

uint64_t Transaction::duration() const {
    uint64_t min_ = std::numeric_limits<uint64_t>::max();
    uint64_t max_ = 0;
    for (auto const *p : items_) {
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

void Transaction::add(const std::shared_ptr<QueryObject> &obj) {
    auto p = std::dynamic_pointer_cast<LogItem>(obj);
    if (!p) {
        throw py::value_error(
            fmt::format("{0} is not a valid LogItem", py::str(py::cast(obj)).cast<std::string>()));
    }
    items_.emplace_back(p.get());
    QueryArray::add(obj);
}

Transactions::Transactions(const std::shared_ptr<QueryArray> &array) : QueryArray(*array) {
    // type check for each item
    for (auto const &item : array->data) {
        auto p = std::dynamic_pointer_cast<Transaction>(item);
        if (!p) {
            throw py::value_error(fmt::format("{0} is not a valid LogItem",
                                              py::str(py::cast(item)).cast<std::string>()));
        }
        transactions_[p->items_[0]->get_item().time].emplace_back(p.get());
    }
}

void Transactions::add(const std::shared_ptr<QueryObject> &obj) {
    auto p = std::dynamic_pointer_cast<Transaction>(obj);
    if (!p) {
        throw py::value_error(
            fmt::format("{0} is not a valid LogItem", py::str(py::cast(obj)).cast<std::string>()));
    }
    transactions_[p->items_[0]->get_item().time].emplace_back(p.get());
    QueryArray::add(obj);
}

std::shared_ptr<Transactions> trans_seq(  // NOLINT
    const std::shared_ptr<Transactions> &trans, const std::shared_ptr<Transactions> &other,
    const std::function<bool(const std::shared_ptr<QueryObject> &,
                             const std::shared_ptr<QueryObject> &)> &predicate,
    uint64_t window) {
    auto result = std::make_shared<Transactions>(trans->ooze);
    // we copy the second map here
    auto other_map = other->transactions_;
    for (auto const &[time, t] : trans->transactions_) {
        auto start_time = other_map.lower_bound(time);
        while (start_time != other_map.end() && ((start_time->first) < (time + window))) {
            auto const &other_t = start_time->second;
            for (auto *target_t : t) {
                for (auto *other_item : other_t) {
                    if (other_item->size() != 1) {
                        throw py::value_error("Invalid transaction item");
                    }
                    auto o = other_item->get(0)->shared_from_this();
                    bool r = predicate(
                        target_t->size() == 1 ? target_t->get(0) : target_t->shared_from_this(), o);
                    if (!r) continue;
                    auto result_entry = std::make_shared<Transaction>(*target_t);
                    result_entry->add(o);
                    result->add(result_entry);
                }
            }
            start_time++;
        }
    }
    return result;
}

void init_transaction(py::module &m) {
    auto t = py::class_<Transaction, QueryArray, std::shared_ptr<Transaction>>(m, "Transaction");
    t.def(py::init<uint64_t, const std::shared_ptr<QueryArray> &>(), py::arg("id"),
          py::arg("array"));
    t.def_property_readonly("duration", &Transaction::duration);

    t.def_readwrite("id", &Transaction::id);

    m.def(
        "to_transaction",
        [](const std::shared_ptr<LogItem> &item) {
            auto r = std::make_shared<Transaction>(0, item->ooze);
            r->add(item);
            return r;
        },
        py::arg("log_item"));

    auto ts =
        py::class_<Transactions, QueryArray, std::shared_ptr<Transactions>>(m, "Transactions");
    ts.def("seq", &trans_seq, py::arg("other"), py::arg("predicate"), py::arg("window"));
    ts.def(py::init([](const std::shared_ptr<QueryArray> &obj) {
        auto transactions = std::make_shared<Transactions>(obj);
        return transactions;
    }));
}
