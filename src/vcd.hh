#ifndef HGDB_RTL_VCD_HH
#define HGDB_RTL_VCD_HH

#include <vcd/vcd.hh>
#include <map>
#include <memory>
#include <set>

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
};

}

#endif  // HGDB_RTL_VCD_HH
