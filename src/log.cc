#include "log.hh"

namespace hgdb::log {

LogFile::LogFile(const std::string &filename) : path(filename) {
    stream = std::ifstream(filename);
    if (stream.bad()) {
        throw std::runtime_error("Invalid filename " + filename);
    }
}

LogFile::~LogFile() { stream.close(); }

void LogDatabase::add_file(const std::string &filename) {
    log_files_.emplace_back(std::make_unique<LogFile>(filename));
}

std::unique_ptr<LogItemBatch> compress(const LogPrintfParser::Format &format,
                                       std::vector<LogItem> &items,
                                       std::map<uint64_t, std::vector<uint64_t>> &item_index_,
                                       uint64_t batch_idx) {
    // we perform column based storage
    if (items.empty()) return nullptr;
    for (auto const &item: items) {

    }
    return nullptr;
}

void LogDatabase::parse(const LogPrintfParser &parser) {
    std::vector<LogItem> batch;
    batch.reserve(batch_size_);
    format_ = parser.format();

    for (auto &file_ptr : log_files_) {
        auto *file = file_ptr.get();
        // we first parse the file to create raw files
        std::string line;
        if (file->stream.bad()) return;
        // index the positions
        while (std::getline(file->stream, line)) {
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

}  // namespace hgdb::log
