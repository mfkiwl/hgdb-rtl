#include "log.hh"

#include <type_traits>

namespace hgdb::log {

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

void LogDatabase::add_file(const std::string &filename) {
    log_files_.emplace_back(std::make_unique<LogFile>(filename));
}

void LogDatabase::add_file(std::istream &stream) {
    log_files_.emplace_back(std::make_unique<LogFile>(stream));
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

    std::vector data = raw_data_;

    auto times = deserialize<uint64_t>(data, pos);
    auto int_values = deserialize<int64_t>(data, pos);
    auto float_values = deserialize<double>(data, pos);
    auto str_values = deserialize<std::string>(data, pos);

    // based on the format
    uint64_t int_size = 0, float_size = 0, str_size = 0;
    for (auto const &[name, entry] : format_) {
        auto [t, idx] = entry;
        switch (t) {
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
    }
}

std::unique_ptr<LogItemBatch> compress(const LogPrintfParser::Format &format,
                                       std::vector<LogItem> &items,
                                       std::map<uint64_t, std::set<uint64_t>> &item_index_,
                                       uint64_t batch_idx) {
    // we perform column based storage
    if (items.empty()) return nullptr;
    for (auto const &item : items) {
        item_index_[item.time].emplace(batch_idx);
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

    auto ptr = std::make_unique<LogItemBatch>(items.size(), uncompressed_data, format);
    items.clear();
    return ptr;
}

void LogDatabase::parse(LogFormatParser &parser) {
    std::vector<LogItem> batch;
    batch.reserve(batch_size_);
    format_ = parser.format();

    for (auto &file_ptr : log_files_) {
        auto *file = file_ptr.get();
        // we first parse the file to create raw files
        std::string line;
        if (file->stream->bad()) return;
        // index the positions
        while (std::getline(*file->stream, line)) {
            if (line.empty()) continue;
            auto item = parser.parse(line);
            batch.emplace_back(item);

            if (batch.size() >= batch_size_) {
                auto p = compress(format_, batch, item_index_, batches_.size());
                if (p) batches_.emplace_back(std::move(p));
            }
        }
    }
    auto p = compress(format_, batch, item_index_, batches_.size());
    if (p) batches_.emplace_back(std::move(p));
}

std::vector<LogItemBatch *> LogDatabase::get_batch(uint64_t time) const {
    if (item_index_.find(time) == item_index_.end()) return {};
    std::vector<LogItemBatch *> result;
    auto const &values = item_index_.at(time);
    result.reserve(values.size());
    for (auto const idx : values) {
        result.emplace_back(batches_[idx].get());
    }

    return result;
}

}  // namespace hgdb::log
