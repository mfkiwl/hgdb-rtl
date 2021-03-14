#ifndef HGDB_RTL_TRANSACTION_HH
#define HGDB_RTL_TRANSACTION_HH

#include "log.hh"
#include "object.hh"

class Transaction : public QueryArray {
public:
    uint64_t id;
    Transaction(uint64_t id, const std::shared_ptr<QueryArray> &items);
    Transaction(uint64_t id, Ooze *ooze) : QueryArray(ooze), id(id) {}
    [[nodiscard]] uint64_t duration() const;
    void add(const std::shared_ptr<QueryObject> &obj) override;

    std::vector<LogItem *> items_;
};

class Transactions : public QueryArray {
public:
    explicit Transactions(Ooze *ooze) : QueryArray(ooze) {}
    explicit Transactions(const std::shared_ptr<QueryArray> &array);
    void add(const std::shared_ptr<QueryObject> &obj) override;

    // index on time
    std::map<uint64_t, std::vector<Transaction *>> transactions_;
};

#endif  // HGDB_RTL_TRANSACTION_HH
