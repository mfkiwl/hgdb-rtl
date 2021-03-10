#ifndef HGDB_RTL_LOG_HH
#define HGDB_RTL_LOG_HH

#include <fstream>
#include <map>
#include <memory>
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

// since logs are semi-structured. we only pick a few attributes that share among different log
// structures. Notice that log items are directly instantiated from the io locations
class LogItem {
public:
    LogItem() = default;
    explicit LogItem(uint64_t time) : time(time) {}
    uint64_t time = 0;

    // pointing to the log file
    LogFile *file = nullptr;

    virtual ~LogItem() = default;

    // we support following types
    std::vector<int64_t> int_values;
    std::vector<std::string> str_values;
    std::vector<double> float_values;
};

class LogFormatParser {
public:
    enum class ValueType { Int, Str, Float };
    using Format = std::map<std::string, std::pair<ValueType, uint64_t>>;
    [[nodiscard]] virtual Format format() const = 0;
    [[nodiscard]] virtual LogItem parse(const std::string &content) = 0;
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

    std::map<std::string, std::pair<ValueType, uint64_t>> format() const override;
};

class LogDatabase {
public:
    LogDatabase() = default;
    explicit LogDatabase(uint64_t batch_size) : batch_size_(batch_size) {}

    void add_file(const std::string &filename);
    void add_file(std::istream &stream);
    void parse(LogFormatParser &parser);
    [[nodiscard]] std::vector<LogItemBatch *> get_batch(uint64_t time) const;

private:
    uint64_t batch_size_ = 1024;
    std::vector<std::unique_ptr<LogItemBatch>> batches_;
    std::map<uint64_t, std::set<uint64_t>> item_index_;
    std::vector<std::unique_ptr<LogFile>> log_files_;
    LogFormatParser::Format format_;
};

}  // namespace hgdb::log

#endif  // HGDB_RTL_LOG_HH
