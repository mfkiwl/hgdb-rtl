#ifndef HGDB_RTL_OBJECT_HH
#define HGDB_RTL_OBJECT_HH

#include "../../src/rtl.hh"

struct QueryObject {
public:
    virtual ~QueryObject() = default;
    virtual std::shared_ptr<QueryObject> map(
        const std::function<std::shared_ptr<QueryObject>(QueryObject *)> &mapper);
};

struct QueryArray : public QueryObject {
public:
    std::shared_ptr<QueryObject> map(
        const std::function<std::shared_ptr<QueryObject>(QueryObject *)> &mapper) override;

    [[nodiscard]] virtual uint64_t size() const { return list_.size(); }
    [[nodiscard]] virtual QueryObject * get(uint64_t idx) const { return list_[idx].get(); }

protected:
    virtual std::vector<QueryObject *> list();
    virtual void add(const std::shared_ptr<QueryObject> &obj);

private:
    // default rtl_list holder
    // child class can implement their own rtl_list holders
    std::vector<std::shared_ptr<QueryObject>> list_;
};

#endif  // HGDB_RTL_OBJECT_HH
