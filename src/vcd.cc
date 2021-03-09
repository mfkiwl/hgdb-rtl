#include "vcd.hh"

#include <vector>

#include "fmt/format.h"
#include "vcd/vcd.hh"

namespace hgdb::vcd {

std::string VCDSignal::get_value(uint64_t time) const {
    // map is a b-tree
    auto const &ref_time = raw_values->lower_bound(time);
    if (ref_time != raw_values->end()) {
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

    // aliased mapping
    std::unordered_map<std::string, std::string> identifier_mapping;
    // true identifier to aliased identifier
    std::unordered_set<std::string> seen_identifiers;
    // identifier to signals
    std::unordered_map<std::string, VCDSignal *> identifier_signal_mapping;
    // how many times has an identifier value changed
    std::unordered_map<std::string, uint64_t> vcd_count;

    parser.set_on_var_def([&hierarchy, &identifier_signal_mapping, this](const VCDVarDef &def) {
        // need to compute path
        auto path = get_path_name(hierarchy, def.name);
        auto signal = std::make_shared<VCDSignal>();
        signal->identifier = def.identifier;
        signal->path = path;
        signal->name = def.name;
        signal->db = this;
        signal->raw_values = nullptr;
        signals.emplace(path, signal);
        identifier_signal_mapping.emplace(def.identifier, signal.get());
    });

    parser.set_value_change([&identifier_mapping, &seen_identifiers, &identifier_signal_mapping,
                             &vcd_count, this](const VCDValue &value) {
        alias_signal(identifier_mapping, seen_identifiers, identifier_signal_mapping, value);
        if (vcd_count.find(value.identifier) == vcd_count.end())
            vcd_count.emplace(value.identifier, 0);
        vcd_count[value.identifier]++;
    });

    parser.set_on_time_change(
        [&vcd_count, &identifier_mapping, &identifier_signal_mapping, this](uint64_t time) {
            times.emplace(time);
            de_alias_signal(vcd_count, identifier_mapping, identifier_signal_mapping);
        });

    parser.parse();
}

void VCDDatabase::alias_signal(
    std::unordered_map<std::string, std::string> &identifier_mapping,
    std::unordered_set<std::string> &seen_identifiers,
    std::unordered_map<std::string, VCDSignal *> &identifier_signal_mapping,
    const VCDValue &value) {
    // need to figure out whether the signals should be aliased
    std::string identifier = value.identifier;
    if (identifier_mapping.empty()) {
        // map to itself
        identifier_mapping.emplace(identifier, identifier);
    } else if (identifier_mapping.find(identifier) != identifier_mapping.end()) {
        // we have found an alias mapping
        identifier = identifier_mapping.at(identifier);
    } else {
        // this is a new one. we just randomly pick up an identifier
        // that has the same value
        for (auto const &[id, vs] : values) {
            if (vs.size() == 1 && vs.find(value.time) != vs.end() &&
                vs.at(value.time) == value.value) {
                identifier = id;
                break;
            }
        }
        // can't find any, randomly pick one
        if (identifier.empty()) identifier = identifier_mapping.begin()->second;
    }
    auto &values_ = values[identifier];
    auto *sig = identifier_signal_mapping.at(value.identifier);
    sig->raw_values = &values_;

    // check if the time exists or not
    if (values_.find(value.time) == values_.end()) {
        values_.emplace(value.time, value.value);
        seen_identifiers.emplace(value.identifier);
        return;
    }
    // if this value already exists
    // couple things to check.
    // first, is this the first time we seen this new identifier?
    //        if so, we need to check the values should be size 1
    // second, does the current value match?
    bool should_alias = true;
    if (seen_identifiers.find(value.identifier) == seen_identifiers.end()) {
        if (values_.size() > 1) should_alias = false;
        if (values_.size() == 1 && values_.begin()->first != value.time) should_alias = false;
    }
    if (should_alias) {
        if (values_.find(value.time) != values_.end() && values_.at(value.time) != value.value) {
            should_alias = false;
        }
    }
    seen_identifiers.emplace(value.identifier);
    // if we are aliasing
    if (should_alias) {
        identifier_mapping[value.identifier] = identifier;
        return;
    }

    // copy any values from the values < the current time
    for (auto const &[t, v] : values_) {
        if (t < value.time) values[value.identifier].emplace(t, v);
    }
    values[value.identifier].emplace(value.time, value.value);
    sig->raw_values = &values.at(value.identifier);
    identifier_mapping[value.identifier] = value.identifier;
}

void VCDDatabase::de_alias_signal(
    std::unordered_map<std::string, uint64_t> &vcd_count,
    std::unordered_map<std::string, std::string> &identifier_mapping,
    std::unordered_map<std::string, VCDSignal *> &identifier_signal_mapping) {
    // we need to detect if we should de-alias some signals
    std::unordered_set<std::string> changes;
    for (auto const &[from, to] : identifier_mapping) {
        if (from != to) {
            auto from_count = vcd_count.at(from);
            auto to_count = vcd_count.at(to);
            if (from_count != to_count) {
                // need to de-alias these two signals
                changes.emplace(from);
            }
        }
    }

    for (auto const &identifier : changes) {
        // if there is an signal that matches out signal values
        // we should alias to that instead
        auto count = vcd_count.at(identifier);
        auto old_identifier = identifier_mapping.at(identifier);
        auto iter = values.at(old_identifier).begin();
        std::map<uint64_t, std::string> new_values;
        // notice that map (B-tree) is ordered nicely
        for (uint64_t i = 0; i < count; i++) {
            new_values.emplace(iter->first, iter->second);
            iter++;
        }

        bool match = true;
        std::string match_id;
        for (auto const &[id, vs] : values) {
            if (vs.size() == count) {
                // check values as well
                for (auto const &[t, v] : new_values) {
                    if (vs.find(t) == vs.end() || vs.at(t) != v) {
                        match = false;
                        break;
                    }
                }
            }
            if (match) {
                match_id = id;
                break;
            }
        }

        if (!match) {
            // actually create new values
            values[identifier] = new_values;
            // remap the symbol
            identifier_signal_mapping.at(identifier)->raw_values = &values[identifier];
            identifier_mapping[identifier] = identifier;
        } else {
            identifier_signal_mapping.at(identifier)->raw_values = &values[match_id];
            identifier_mapping[identifier] = match_id;
            if (values.find(identifier) != values.end()) {
                values.erase(identifier);
            }
        }
    }
}

std::map<std::string, uint64_t> VCDDatabase::get_stats() const {
    // compute the size of each values
    uint64_t total_size = 0, before_size = 0;
    for (auto const &[name, value] : values) {
        for (auto const &[t, s] : value) {
            total_size += s.length();
            before_size += s.length();
        }
        total_size += sizeof(value);  // NOLINT
    }
    total_size += sizeof(values);  // NOLINT
    // compute number of aliased signal
    uint64_t num_aliased = signals.size() - values.size();

    // compute the compression ratio
    uint64_t after_size = 0;
    for (auto const &[p, sig] : signals) {
        auto const &value = *sig->raw_values;
        for (auto const &[t, s] : value) {
            after_size += s.length();
        }
    }

    return {{"total_size", total_size},
            {"num_aliased", num_aliased},
            {"before_size", before_size},
            {"after_size", after_size}};
}

}  // namespace hgdb::vcd