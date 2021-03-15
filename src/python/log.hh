#ifndef HGDB_RTL_PYTHON_LOG_HH
#define HGDB_RTL_PYTHON_LOG_HH

#include "../log.hh"
#include "data_source.hh"

class LogItem : public QueryObject {
public:
    LogItem(Ooze *ooze, hgdb::log::LogDatabase *db, const hgdb::log::LogIndex &index)
        : QueryObject(ooze), db(db), index(index) {}
    hgdb::log::LogDatabase *db;
    hgdb::log::LogIndex index;
    std::map<std::string, pybind11::object> values() const override;

    hgdb::log::LogItem get_item() const;

    static void clear_cache() { cached_items_.clear(); }

private:
    static std::map<hgdb::log::LogIndex, hgdb::log::LogItem> cached_items_;
    auto static constexpr cache_size_ = 1 << 20;
};

class Log : public DataSource {
public:
    Log();
    [[nodiscard]] std::vector<py::handle> provides() const override;

    std::shared_ptr<QueryArray> get_selector(py::handle handle) override;
    std::shared_ptr<QueryObject> bind(const std::shared_ptr<QueryObject> &,
                                      const py::object &) override {
        return nullptr;
    }

    void add_file(const std::string &filename,
                  const std::shared_ptr<hgdb::log::LogFormatParser> &parser);

    void on_added(Ooze *ooze) override;

private:
    Ooze *ooze_ = nullptr;
    std::vector<std::pair<std::string, std::shared_ptr<hgdb::log::LogFormatParser>>> files_;
    std::unique_ptr<hgdb::log::LogDatabase> db_;
    std::vector<std::shared_ptr<hgdb::log::LogFormatParser>> parsers_;
    // record the batch indices
    std::vector<std::set<uint64_t>> parser_batches_;
};

#endif  // HGDB_RTL_PYTHON_LOG_HH
