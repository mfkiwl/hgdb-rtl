#include "log.hh"

#include <limits>
#include <type_traits>

#include "lz/lz.hh"

namespace hgdb::log {

constexpr int compression_level = 0;

LogFile::LogFile(const std::string &filename) : path(filename) {
    fstream = std::ifstream(filename);
    stream = &fstream;
    if (stream->bad()) {
        throw std::runtime_error("Invalid filename " + filename);
    }
}

LogFile::~LogFile() {
    if (!path.empty()) {
        fstream.close();
    }
}

LogPrintfParser::LogPrintfParser(const std::string &format,
                                 const std::vector<std::string> &attr_names)
    : time_index_(std::numeric_limits<uint64_t>::max()) {
    parse_format(format);
    if (types_.size() != (attr_names.size() + 1) ||
        time_index_ == std::numeric_limits<uint64_t>::max()) {
        error_ = true;
        return;
    }
    // set up the format
    uint64_t int_values = 0, float_values = 0, str_values = 0;
    uint64_t type_idx = 0;
    for (uint64_t i = 0; i < attr_names.size(); i++) {
        auto type = types_[type_idx++];
        auto const &name = attr_names[i];
        uint64_t index;
        bool add_format = true;
        switch (type) {
            case ValueType::Int:
            case ValueType::Hex: {
                index = int_values++;
                break;
            }
            case ValueType::Float: {
                index = float_values++;
                break;
            }
            case ValueType::Str: {
                index = str_values++;
                break;
            }
            default: {
                // we don't record about time
                add_format = false;
                i--;
            }
        }
        if (add_format) this->format.emplace(name, std::make_pair(type, index));
    }
}

void LogPrintfParser::parse_format(const std::string &format) {
    // we are interested in any $display related formatting
    // hand-rolled FSM-based parser
    int state = 0;
    std::string regex_data;
    regex_data.reserve(format.size() * 2);

    for (auto c : format) {
        if (state == 0) {
            if (c == '\\') {
                // escape mode
                state = 2;
            } else if (c == '%') {
                state = 1;
            } else {
                regex_data.append(std::string(1, c));
            }
        } else if (state == 1) {
            if (isdigit(c)) {
                continue;
            } else if (c == 'd') {
                // put number regex here
                types_.emplace_back(ValueType::Int);
                regex_data.append(R"(\s?(\d+))");
            } else if (c == 't') {
                types_.emplace_back(ValueType::Time);
                regex_data.append(R"(\s?(\d+))");
                time_index_ = types_.size();
            } else if (c == 'x' || c == 'X') {
                types_.emplace_back(ValueType::Hex);
                regex_data.append(R"(\s?([\da-fA-F]+))");
            } else if (c == 's') {
                types_.emplace_back(ValueType::Str);
                regex_data.append(R"((\w+))");
            } else if (c == 'm') {
                types_.emplace_back(ValueType::Str);
                regex_data.append(R"(([\w$_\d.]+))");
            } else if (c == 'f') {
                types_.emplace_back(ValueType::Float);
                regex_data.append(R"(([+-]?([0-9]*[.])?[0-9]+))");
            } else {
                throw std::runtime_error("Unknown formatter " + std::string(1, c));
            }
            state = 0;
        } else {
            if (c == '%') {
                // no need to escape this at all
                regex_data.append(std::string(1, c));
            } else {
                regex_data.append(std::string(1, '\\'));
                regex_data.append(std::string(1, c));
            }
            state = 0;
        }
    }
    // set the regex
    re_ = std::regex(regex_data);
}

LogItem LogPrintfParser::parse(const std::string &content) {
    auto log = LogItem();
    if (has_error()) {
        return log;
    }
    std::smatch matches;

    if (std::regex_search(content, matches, re_)) {
        for (auto i = 1u; i < matches.size(); i++) {
            // need to convert the types
            auto idx = i - 1;
            auto type = types_[idx];
            auto const &match = matches[i];
            switch (type) {
                case ValueType::Int: {
                    auto value = std::stol(match.str());
                    log.int_values.emplace_back(value);
                    break;
                }
                case ValueType::Time: {
                    auto value = std::stol(match.str());
                    log.time = value;
                    break;
                }
                case ValueType::Hex: {
                    auto value = std::stol(match.str(), nullptr, 16);
                    log.int_values.emplace_back(value);
                    break;
                }
                case ValueType::Float: {
                    auto value = std::stof(match.str());
                    log.float_values.emplace_back(value);
                    break;
                }
                case ValueType::Str: {
                    log.str_values.emplace_back(match.str());
                    break;
                }
            }
        }
    }
    return log;
}

template <typename T>
void write_data(std::vector<char> &data, uint64_t &pos, T v) {
    auto const *raw_value = reinterpret_cast<char *>(&v);
    for (uint64_t i = 0; i < sizeof(T); i++) {
        data[pos + i] = raw_value[i];
    }
    pos += sizeof(T);
}

template <typename T>
T read_data(const std::vector<char> &data, uint64_t &pos) {
    const char *raw_ptr = data.data() + pos;
    auto const *ptr = reinterpret_cast<const T *>(raw_ptr);
    T v = *ptr;
    pos += sizeof(T);
    return v;
}

template <typename T>
void serialize(std::vector<char> &data, const std::vector<T> &values) {
    uint64_t pos = data.size();
    auto array_size = values.size();
    data.resize(data.size() + sizeof(array_size));
    write_data(data, pos, array_size);

    if constexpr (std::is_same<std::string, T>::value) {
        // we use old school C style char
        uint64_t total_size = 0;
        for (auto const &v : values) {
            total_size += 1 + v.size();
        }
        data.resize(data.size() + total_size);
        for (auto const &v : values) {
            for (auto const c : v) {
                data[pos++] = c;
            }
            data[pos++] = '\0';
        }

    } else {
        data.resize(data.size() + values.size() * sizeof(T));
        for (auto const v : values) {
            write_data(data, pos, v);
        }
    }
}

template <typename T>
std::vector<T> deserialize(const std::vector<char> &data, uint64_t &pos) {
    auto size = read_data<uint64_t>(data, pos);
    std::vector<T> result(size);
    if constexpr (std::is_same<std::string, T>::value) {
        for (uint64_t i = 0; i < size; i++) {
            result[i] = std::string(data.data() + pos);
            pos += result[i].size() + 1;
        }
    } else {
        for (uint64_t i = 0; i < size; i++) {
            result[i] = read_data<T>(data, pos);
        }
    }
    return result;
}

void LogItemBatch::get_items(const std::vector<LogItem *> &items) const {
    // we first decompress each column
    uint64_t pos = 0;

    std::vector data = lz::decompress(raw_data_);

    auto times = deserialize<uint64_t>(data, pos);
    auto int_values = deserialize<int64_t>(data, pos);
    auto float_values = deserialize<double>(data, pos);
    auto str_values = deserialize<std::string>(data, pos);

    // based on the format
    uint64_t int_size = 0, float_size = 0, str_size = 0;
    for (auto const &[name, entry] : format_) {
        auto [t, idx] = entry;
        switch (t) {
            case LogFormatParser::ValueType::Hex:
            case LogFormatParser::ValueType::Int: {
                int_size++;
                break;
            }
            case LogFormatParser::ValueType::Float: {
                float_size++;
                break;
            }
            case LogFormatParser::ValueType::Str: {
                str_size++;
                break;
            }
            case LogFormatParser::ValueType::Time: {
                // don't care
                break;
            }
        }
    }
    uint64_t int_pos = 0, float_pos = 0, str_pos = 0;
    for (uint64_t i = 0; i < size_; i++) {
        items[i]->time = times[i];
        items[i]->int_values =
            std::vector(int_values.begin() + int_pos, int_values.begin() + int_pos + int_size);
        items[i]->float_values = std::vector(float_values.begin() + float_pos,
                                             float_values.begin() + float_pos + float_size);
        items[i]->str_values =
            std::vector(str_values.begin() + str_pos, str_values.begin() + str_pos + str_size);

        int_pos += int_size;
        float_pos += float_size;
        str_pos += str_size;
        items[i]->format = &format_;
    }
}

std::unique_ptr<LogItemBatch> compress(const LogPrintfParser::Format &format,
                                       std::vector<LogItem> &items,
                                       std::vector<std::shared_ptr<LogIndex>> &item_index_,
                                       uint64_t batch_idx) {
    // we perform column based storage
    if (items.empty()) return nullptr;
    for (uint64_t i = 0; i < items.size(); i++) {
        auto entry = std::make_shared<LogIndex>(batch_idx, i);
        item_index_.emplace_back(entry);
    }
    // need to construct data array
    std::vector<char> uncompressed_data;
    // we do column storage
    constexpr uint64_t entry_per_item = 2;

    std::vector<uint64_t> times;
    times.reserve(items.size());
    for (auto const &item : items) times.emplace_back(item.time);
    serialize(uncompressed_data, times);

    std::vector<int64_t> int_values;
    int_values.reserve(items.size() * entry_per_item);
    for (auto const &item : items)
        int_values.insert(int_values.end(), item.int_values.begin(), item.int_values.end());
    serialize(uncompressed_data, int_values);

    std::vector<double> float_values;
    float_values.reserve(items.size() * entry_per_item);
    for (auto const &item : items)
        float_values.insert(float_values.end(), item.float_values.begin(), item.float_values.end());
    serialize(uncompressed_data, float_values);

    std::vector<std::string> str_values;
    str_values.reserve(items.size() * entry_per_item);
    for (auto const &item : items)
        str_values.insert(str_values.end(), item.str_values.begin(), item.str_values.end());
    serialize(uncompressed_data, str_values);

    auto compressed_data = lz::compress(uncompressed_data, compression_level);

    auto ptr = std::make_unique<LogItemBatch>(items.size(), compressed_data, format);
    items.clear();
    return ptr;
}

void LogDatabase::parse(const std::string &filename, LogFormatParser &parser) {
    LogFile file(filename);
    parse(file, parser);
}

void LogDatabase::parse(std::istream &stream, LogFormatParser &parser) {
    LogFile file(stream);
    parse(file, parser);
}

void LogDatabase::parse(LogFile &file, LogFormatParser &parser) {
    std::vector<LogItem> batch;
    batch.reserve(batch_size_);
    formats_.emplace_back(parser.format);
    auto *format = &formats_.back();

    // we first parse the file to create raw files
    std::string line;
    if (file.stream->bad()) return;
    // index the positions
    while (std::getline(*file.stream, line)) {
        if (line.empty()) continue;
        auto item = parser.parse(line);
        item.format = format;
        batch.emplace_back(item);

        if (batch.size() >= batch_size_) {
            auto p = compress(*format, batch, item_index_, batches_.size());
            if (p) batches_.emplace_back(std::move(p));
        }
    }

    auto p = compress(*format, batch, item_index_, batches_.size());
    if (p) batches_.emplace_back(std::move(p));
}

void LogDatabase::get_item(LogItem *item, const LogIndex &index) {
    if (cached_index_ && *cached_index_ == index.batch_index) {
        // use cached index
        *item = cached_items_[index.index];
    } else {
        // need to decode it
        auto &batch = batches_[index.batch_index];
        cached_index_ = index.batch_index;
        cached_items_.resize(batch->size());
        std::vector<LogItem *> items;
        items.resize(batch->size());
        for (uint64_t i = 0; i < items.size(); i++) items[i] = &cached_items_[i];
        batch->get_items(items);
        *item = cached_items_[index.index];
    }
}

}  // namespace hgdb::log
