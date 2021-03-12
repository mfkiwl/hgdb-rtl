#ifndef HGDB_RTL_LOG_HH
#define HGDB_RTL_LOG_HH

#include <fstream>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace hgdb::log {

class LogFile {
public:
    explicit LogFile(const std::string &filename);
    explicit LogFile(std::istream &s) : stream(&s) {}

    std::istream *stream;
    std::ifstream fstream;
    std::string path;

    ~LogFile();
};

class LogItem;
class LogFormatParser {
public:
    enum class ValueType { Int, Hex, Str, Float, Time };
    using Format = std::map<std::string, std::pair<ValueType, uint64_t>>;
    [[nodiscard]] virtual LogItem parse(const std::string &content) = 0;

    LogFormatParser::Format format;
};

// since logs are semi-structured. we only pick a few attributes that share among different log
// structures. Notice that log items are directly instantiated from the io locations
class LogItem {
public:
    LogItem() = default;
    explicit LogItem(uint64_t time) : time(time) {}
    uint64_t time = 0;

    virtual ~LogItem() = default;

    // we support following types
    std::vector<int64_t> int_values;
    std::vector<std::string> str_values;
    std::vector<double> float_values;

    const LogFormatParser::Format *format = nullptr;
};

// a batch of log items
class LogItemBatch {
public:
    LogItemBatch(uint64_t size, std::vector<char> raw_data, const LogFormatParser::Format &format)
        : size_(size), raw_data_(std::move(raw_data)), format_(format) {}

    void get_items(const std::vector<LogItem *> &items) const;

    [[nodiscard]] uint64_t size() const { return size_; }

private:
    uint64_t size_;
    std::vector<char> raw_data_;
    const LogFormatParser::Format &format_;
};

class LogPrintfParser : public LogFormatParser {
public:
    LogPrintfParser(const std::string &format, const std::vector<std::string> &attr_names);

    LogItem parse(const std::string &content) override;

    [[nodiscard]] bool has_error() const { return error_; }

private:
    void parse_format(const std::string &format);
    std::regex re_;
    bool error_ = false;
    uint64_t time_index_;
    std::vector<ValueType> types_;
};

struct LogIndex {
public:
    LogIndex(uint64_t batch_index, uint64_t index) : batch_index(batch_index), index(index) {}
    uint64_t batch_index;
    uint64_t index;

    bool operator<(const LogIndex &other) const {
        auto self = batch_index << 32u | index;
        auto o = other.batch_index << 32u | other.index;
        return self < o;
    }
};

class LogDatabase {
public:
    LogDatabase() = default;
    explicit LogDatabase(uint64_t batch_size) : batch_size_(batch_size) {}

    std::set<uint64_t> parse(const std::string &filename, LogFormatParser &parser);
    std::set<uint64_t> parse(std::istream &stream, LogFormatParser &parser);
    [[nodiscard]] const std::vector<std::shared_ptr<LogIndex>> &item_index() const {
        return item_index_;
    }

    void get_item(LogItem *item, const LogIndex &index);

private:
    uint64_t batch_size_ = 1024;
    std::vector<std::unique_ptr<LogItemBatch>> batches_;
    std::vector<std::shared_ptr<LogIndex>> item_index_;

    // we cache one decoded batch
    std::optional<uint64_t> cached_index_;
    std::vector<LogItem> cached_items_;

    std::set<uint64_t> parse(LogFile &file, LogFormatParser &parser);

    std::vector<LogFormatParser::Format> formats_;
};

}  // namespace hgdb::log

#endif  // HGDB_RTL_LOG_HH
