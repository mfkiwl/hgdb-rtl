#ifndef HGDB_RTL_OBJECT_HH
#define HGDB_RTL_OBJECT_HH

#include "../../src/rtl.hh"

template <class T>
inline const std::type_info &get_type_info() {
    return typeid(T);
}

struct QueryObject {
public:
    virtual ~QueryObject() = default;
    virtual std::shared_ptr<QueryObject> map(
        const std::function<std::shared_ptr<QueryObject>(QueryObject *)> &mapper);
    [[nodiscard]] virtual const std::type_info &type_info() const {
        return get_type_info<QueryObject>();
    }
};

struct QueryArray : public QueryObject {
public:
    std::shared_ptr<QueryObject> map(
        const std::function<std::shared_ptr<QueryObject>(QueryObject *)> &mapper) override;

    [[nodiscard]] virtual uint64_t size() const { return list_.size(); }
    [[nodiscard]] virtual std::shared_ptr<QueryObject> get(uint64_t idx) const {
        return list_[idx];
    }
    virtual void add(const std::shared_ptr<QueryObject> &obj);

    [[nodiscard]] inline const std::type_info &type_info() const override {
        return get_type_info<QueryArray>();
    }

protected:
    virtual std::vector<QueryObject *> list();

private:
    // default rtl_list holder
    // child class can implement their own rtl_list holders
    std::vector<std::shared_ptr<QueryObject>> list_;
};

#endif  // HGDB_RTL_OBJECT_HH
