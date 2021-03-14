#ifndef HGDB_RTL_TRANSACTION_HH
#define HGDB_RTL_TRANSACTION_HH

#include "object.hh"

class Transaction: public QueryArray {
public:
    uint64_t id;
    Transaction(uint64_t id, const std::shared_ptr<QueryArray> &items);
    [[nodiscard]] uint64_t duration() const;
};

#endif  // HGDB_RTL_TRANSACTION_HH
