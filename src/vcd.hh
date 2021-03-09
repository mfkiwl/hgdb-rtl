#ifndef HGDB_RTL_VCD_HH
#define HGDB_RTL_VCD_HH

#include <map>
#include <memory>
#include <set>
#include <vcd/vcd.hh>

namespace hgdb::vcd {

class VCDDatabase;

// we only care about individual values
class VCDSignal {
public:
    std::string identifier;
    std::string path;
    std::string name;
    // this is ordered
    std::map<uint64_t, std::string> *raw_values;

    std::string get_value(uint64_t time) const;

    uint64_t get_uint_value(uint64_t time) const;

    // backwards pointer to access db
    VCDDatabase *db;
};

class VCDDatabase {
public:
    explicit VCDDatabase(const std::string &filename);

    // if the file is big, we might need to use lazy eval to handle
    // memory usage
    // path -> signal
    std::unordered_map<std::string, std::shared_ptr<VCDSignal>> signals;
    // used to access time we need to query
    std::set<uint64_t> times;
    // hold the actual values
    std::unordered_map<std::string, std::map<uint64_t, std::string>> values;

    std::map<std::string, uint64_t> get_stats() const;

private:
    void alias_signal(std::unordered_map<std::string, std::string> &identifier_mapping,
                      std::unordered_set<std::string> &seen_identifiers,
                      std::unordered_map<std::string, VCDSignal *> &identifier_signal_mapping,
                      const VCDValue &value);
    void de_alias_signal(std::unordered_map<std::string, uint64_t> &vcd_count,
                         std::unordered_map<std::string, std::string> &identifier_mapping,
                         std::unordered_map<std::string, VCDSignal *> &identifier_signal_mapping);
    std::string find_identifier(std::unordered_map<std::string, std::string> &identifier_mapping,
                                const VCDValue &value);
    static std::unordered_set<std::string> identify_signals(
        std::unordered_map<std::string, uint64_t> &vcd_count,
        std::unordered_map<std::string, std::string> &identifier_mapping);
    std::map<uint64_t, std::string> create_new_values(uint64_t count,
                                                      const std::basic_string<char> &identifier);
};

}  // namespace hgdb::vcd

#endif  // HGDB_RTL_VCD_HH
