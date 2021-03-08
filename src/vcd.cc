#include "vcd.hh"

#include <vector>

#include "fmt/format.h"
#include "vcd/vcd.hh"

namespace hgdb::vcd {

std::string VCDSignal::get_value(uint64_t time) const {
    // map is a b-tree
    auto const &ref_time = raw_values.lower_bound(time);
    if (ref_time != raw_values.end()) {
        return ref_time->second;
    } else {
        return "";
    }
}

uint64_t VCDSignal::get_uint_value(uint64_t time) const {
    std::string raw_value = get_value(time);
    uint64_t bits = raw_value.size();
    uint64_t result = 0;
    for (uint64_t i = 0; i < bits; i++) {
        auto bit = bits - i - 1;
        char v = raw_value[i];
        if (v == '1') {
            result |= 1 << bit;
        } else if (v == 'z' || v == 'x') {
            // invalid value we display 0, which is consistent with Verilator
            return 0;
        }
    }
    return result;
}

std::string get_path_name(const std::vector<std::string> &hierarchy, const std::string &name) {
    return fmt::format("{0}.{1}", fmt::join(hierarchy.begin(), hierarchy.end(), "."), name);
}

VCDDatabase::VCDDatabase(const std::string &filename) {
    // need to parse the file here
    VCDParser parser(filename);
    if (parser.has_error()) {
        return;
    }
    std::vector<std::string> hierarchy;
    // set callbacks
    parser.set_on_enter_scope(
        [&hierarchy](const VCDScopeDef &def) { hierarchy.emplace_back(def.name); });
    parser.set_exit_scope([&hierarchy]() { hierarchy.pop_back(); });
    parser.set_on_var_def([&hierarchy, this](const VCDVarDef &def) {
        // need to compute path
        auto path = get_path_name(hierarchy, def.name);
        auto signal = std::make_shared<VCDSignal>();
        signal->identifier = def.identifier;
        signal->path = path;
        signal->name = def.name;
        signal->db = this;
        signals.emplace(def.identifier, signal);
    });

    parser.set_value_change([this](const VCDValue &value) {
        auto &signal = this->signals.at(value.identifier);
        signal->raw_values.emplace(value.time, value.value);
    });

    parser.set_on_time_change([this](uint64_t time) { times.emplace(time); });

    parser.parse();
}

}  // namespace hgdb::vcd