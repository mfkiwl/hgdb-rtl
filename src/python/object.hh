#ifndef HGDB_RTL_OBJECT_HH
#define HGDB_RTL_OBJECT_HH

#include "../../src/rtl.hh"

struct QueryObject {
public:
    virtual ~QueryObject() = default;
    virtual std::shared_ptr<QueryObject> map(
        const std::function<std::shared_ptr<QueryObject>(QueryObject *)> &mapper);
    [[nodiscard]] virtual std::map<std::string, std::string> values() const { return {}; }
};

struct QueryArray : public QueryObject {
public:
    std::shared_ptr<QueryObject> map(
        const std::function<std::shared_ptr<QueryObject>(QueryObject *)> &mapper) override;

    [[nodiscard]] virtual uint64_t size() const { return data.size(); }
    [[nodiscard]] virtual std::shared_ptr<QueryObject> get(uint64_t idx) const {
        return data[idx];
    }
    virtual void add(const std::shared_ptr<QueryObject> &obj);

    virtual std::vector<std::shared_ptr<QueryObject>>::iterator begin() { return data.begin(); }
    std::vector<std::shared_ptr<QueryObject>>::iterator end() { return data.end(); }

    // default rtl_list holder
    // child class can implement their own rtl_list holders
    std::vector<std::shared_ptr<QueryObject>> data;
};

#endif  // HGDB_RTL_OBJECT_HH
