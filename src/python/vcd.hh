#ifndef HGDB_RTL_PYTHON_VCD_HH
#define HGDB_RTL_PYTHON_VCD_HH

#include "../vcd.hh"
#include "data_source.hh"

struct VCDSignal: public QueryObject {
    std::string path;
    std::string name;
    hgdb::vcd::VCDSignal *signal;

    [[nodiscard]] std::map<std::string, std::string> values() const override;
};


class VCD: public DataSource {
public:
    explicit VCD(const std::string &path);

    [[nodiscard]] inline std::vector<py::handle> provides() const override {
        return {py::type::of<VCDSignal>()};
    }

    std::shared_ptr<QueryArray> get_selector(py::handle handle) override;

private:
    std::unique_ptr<hgdb::vcd::VCDDatabase> db_;
};

#endif  // HGDB_RTL_PYTHON_VCD_HH