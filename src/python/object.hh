#ifndef HGDB_RTL_OBJECT_HH
#define HGDB_RTL_OBJECT_HH

#include "../../src/rtl.hh"
#include "pybind11/pybind11.h"

struct QueryObject {
public:
    virtual ~QueryObject() = default;
    virtual std::shared_ptr<QueryObject> map(
        const std::function<std::shared_ptr<QueryObject>(QueryObject *)> &mapper);
    [[nodiscard]] virtual std::map<std::string, pybind11::object> values() const { return {}; }
    [[nodiscard]] virtual bool is_array() const { return false; }
    [[nodiscard]] virtual std::string str() const { return ""; }
};

struct QueryArray : public QueryObject {
public:
    std::shared_ptr<QueryObject> map(
        const std::function<std::shared_ptr<QueryObject>(QueryObject *)> &mapper) override;

    [[nodiscard]] virtual uint64_t size() const { return data.size(); }
    [[nodiscard]] virtual bool empty() const { return data.empty(); }
    [[nodiscard]] virtual std::shared_ptr<QueryObject> get(uint64_t idx) const { return data[idx]; }
    virtual void add(const std::shared_ptr<QueryObject> &obj);

    virtual std::vector<std::shared_ptr<QueryObject>>::iterator begin() { return data.begin(); }
    std::vector<std::shared_ptr<QueryObject>>::iterator end() { return data.end(); }

    [[nodiscard]] bool is_array() const override { return true; }

    // default rtl_list holder
    // child class can implement their own rtl_list holders
    std::vector<std::shared_ptr<QueryObject>> data;
};

// helper  functions
std::shared_ptr<QueryObject> flatten_size_one_array(const std::shared_ptr<QueryObject> &obj);

#endif  // HGDB_RTL_OBJECT_HH
