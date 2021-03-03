#ifndef HGDB_RTL_OBJECT_HH
#define HGDB_RTL_OBJECT_HH

#include "../../src/rtl.hh"

struct QueryObject {
public:
    virtual ~QueryObject() = default;
    virtual std::unique_ptr<QueryObject> map(
        const std::function<std::unique_ptr<QueryObject>(QueryObject *)> &mapper);
};

struct QueryArray : public QueryObject {
public:
    std::vector<std::unique_ptr<QueryObject>> list;
    std::unique_ptr<QueryObject> map(
        const std::function<std::unique_ptr<QueryObject>(QueryObject *)> &mapper) override;
};

#endif  // HGDB_RTL_OBJECT_HH
