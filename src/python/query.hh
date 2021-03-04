#ifndef HGDB_RTL_QUERY_HH
#define HGDB_RTL_QUERY_HH

#include <functional>
#include "object.hh"

// query related objects
class Filter {
public:
    explicit Filter(std::function<bool(const std::shared_ptr<QueryObject> &)> func): func_(std::move(func)) {};

    std::shared_ptr<QueryObject> apply(const std::shared_ptr<QueryObject> & data);

private:
    std::function<bool(const std::shared_ptr<QueryObject> &)> func_;
};

#endif  // HGDB_RTL_QUERY_HH
