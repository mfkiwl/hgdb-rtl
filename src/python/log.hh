#ifndef HGDB_RTL_PYTHON_LOG_HH
#define HGDB_RTL_PYTHON_LOG_HH

#include "../log.hh"
#include "data_source.hh"

class LogItem : public QueryObject {
public:
    LogItem(Ooze* ooze, hgdb::log::LogDatabase* db, const hgdb::log::LogIndex& index)
        : QueryObject(ooze), db(db), index(index) {}
    hgdb::log::LogDatabase* db;
    hgdb::log::LogIndex index;
    std::map<std::string, pybind11::object> values() const override;

    hgdb::log::LogItem get_item() const;

private:
    static std::map<hgdb::log::LogIndex, hgdb::log::LogItem> cached_items_;
    auto static constexpr cache_size_ = 1 << 20;
};

class Log : public DataSource {
public:
    Log(): DataSource(DataSourceType::Log) {}
    [[nodiscard]] std::vector<py::handle> provides() const override;

private:

};

#endif  // HGDB_RTL_PYTHON_LOG_HH
