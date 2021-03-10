#ifndef HGDB_RTL_LOG_HH
#define HGDB_RTL_LOG_HH

#include <string>
#include <memory>
#include <set>
#include <vector>
#include <fstream>

class LogFile {
public:
    explicit LogFile(const std::string &filename);

    std::ifstream stream;
    std::string path;
};

// since logs are semi-structured. we only pick a few attributes that share among different log
// structures. Notice that log items are directly instantiated from the io locations
class LogItem {
public:
    explicit LogItem(uint64_t time): time(time) {}
    uint64_t time;
    uint64_t start_pos = 0;
    uint64_t end_pos = 0;

    // pointing to the log file
    LogFile *file = nullptr;

    virtual ~LogItem() = default;
};

class LogDatabase {
public:
    LogDatabase() = default;

    void add_file(const std::string &filename);

private:
    // index item by time
    // use map so that we can do B-tree operations
    std::set<uint64_t, std::vector<std::unique_ptr<LogItem>>> items_;
    std::vector<std::unique_ptr<LogFile>> log_files_;
};

#endif  // HGDB_RTL_LOG_HH
