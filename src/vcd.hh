#ifndef HGDB_RTL_VCD_HH
#define HGDB_RTL_VCD_HH

#include <vcd/vcd.hh>
#include <map>
#include <memory>

namespace hgdb::vcd {

// we only care about individual values
class VCDSignal {
public:
    std::string identifier;
    std::string path;
    std::string name;
    // this is ordered
    std::map<uint64_t, std::string> raw_values;

    std::string get_value(uint64_t time);

    uint64_t get_uint_value(uint64_t time);
};


class VCDDatabase {
public:
    explicit VCDDatabase(const std::string &filename);

    // if the file is big, we might need to use lazy eval to handle
    // memory usage
    std::map<std::string, std::shared_ptr<VCDSignal>> signals;
};

}

#endif  // HGDB_RTL_VCD_HH
