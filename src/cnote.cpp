#include "cnote.h"

#include <fmt/format.h>

#include <unordered_set>
#include <filesystem>
#include <vector>
#include <string>

namespace fs = std::filesystem;
using namespace std::string_literals;

/// Wrapper around cstdio FILE*.
///
/// This basically exists to make sure that we don't leave any files open and also
/// to make handling I/O errors easier since we can check for errors in here instead
/// of having to do that in every function that uses a file.
class File {
    FILE* file = nullptr;
    bool readable = false;
    bool writable = false;

public:
    /// Open a new file.
    explicit File() = default;
    explicit File(std::string path, std::string mode = "r") { open(path, mode); }
    explicit File(fs::path path, std::string mode = "r") { open(path.string(), mode); }

    /// Close the file upon destruction.
    ~File() { close(); }

    /// Copying a FILE* is nonsense.
    File(const File&) = delete;
    File& operator=(const File&) = delete;

    /// Move a file.
    File(File&& other) noexcept { *this = std::move(other); }
    File& operator=(File&& other) noexcept {
        // Don't move if we're moving into ourselves.
        if (this == &other) return *this;

        close();
        file = other.file;
        readable = other.readable;
        writable = other.writable;
        other.file = nullptr;
        return *this;
    }

    /// Close a file.
    void close() {
        if (file) {
            fclose(file);
            file = nullptr;
        }

        // Reset file modes.
        readable = writable = false;
    }

    /// Open a file.
    void open(std::string path, std::string mode) {
        close();
        file = fopen(path.data(), mode.data());
        if (!file) { /* TODO: handle error */ }

        // Set file modes.
        readable = mode.find('r') != std::string::npos;
        writable = mode.find_first_of("wa") != std::string::npos;
    }

    /// Print a formatted string to the file.
    template <typename... Args>
    void print(fmt::format_string<Args...> format, Args&&... args) {
        if (!file) { /* TODO: handle error */ }
        if (!writable) { /* TODO: handle error */ }
        fmt::print(file, format, std::forward<Args>(args)...);
    }

    /// Read `size` bytes from the file into `buffer`.
    size_t read(void* buffer, size_t size) {
        if (!file) { /* TODO: handle error */ }
        if (!readable) { /* TODO: handle error */ }
        return fread(buffer, 1, size, file);
    }

    /// Read up to `size` bytes from the file.
    std::string read(size_t size) {
        std::string result;
        result.resize(size);
        auto n = read(result.data(), size);
        result.resize(n);
        return result;
    }
};


Tag::Tag(std::string tag_string) { tag = tag_string; }

Tag& add_tag(std::vector<Tag>& tags, Tag&& new_tag) {
    for (Tag& tag : tags) {
        if (tag.tag == new_tag.tag) {
            return tag;
        }
    }
    new_tag.index = tags.size();
    tags.push_back(std::move(new_tag));
    return tags.back();
}

void add_file_entry(TaggedEntries& data, const fs::path& path) {
    // Check for tags at beginning.

    //fmt::print("{} is a regular file.\n", path.string());
    static constexpr auto whitespace = "\r\n \t\v";
    static constexpr auto tag_marker = "#:";

    File file{path};
    auto contents = file.read(1024);
    contents.shrink_to_fit();

    auto skip_whitespace = [&](std::string& contents) {
        auto pos = contents.find_first_not_of(whitespace);
        if (pos != std::string::npos) {
            contents.erase(0, pos);
        }
    };

    // Skip whitespace at beginnning.
    skip_whitespace(contents);
    // If file has nothing but whitespace, it is not an entry; skip it.
    if (contents.empty()) { return; }

    // Skip C-style comments.
    if (contents.rfind("//", 0) == 0 || contents.rfind("/*", 0) == 0) {
        contents.erase(0, 2);
        // Skip to next whitespace, then begin tag parsing.
        skip_whitespace(contents);
        // If file has nothing but whitespace, it is not an entry; skip it.
        if (contents.empty()) { return; }
    }
    // Skip latex style comments.
    else if (contents.rfind("%", 0) == 0) {
        contents.erase(0, 1);
        skip_whitespace(contents);
        if (contents.empty()) { return; }
    }
    // Skip LISP style comments.
    else if (contents.rfind(";", 0) == 0) {
        while (contents.rfind(";", 0) == 0) {
            contents.erase(0, 1);
            // If file has nothing but whitespace, it is not an entry; skip it.
            if (contents.empty()) { return; }
        }
        skip_whitespace(contents);
        if (contents.empty()) { return; }
    }

    if (contents.rfind(tag_marker, 0) == 0) {
        // Tags are at beginning.
        // Skip past the tag marker.
        contents.erase(0, 2);

        // Start to parse tags white-space separated, up until a newline is met.
        // Find end (first newline)
        auto end_of_tags = contents.find_first_of('\n');
        if (end_of_tags != std::string::npos) {
            contents.erase(end_of_tags, contents.size() - end_of_tags);
        }

        // Create a new entry.
        Entry entry;
        entry.filepath = fs::absolute(path).lexically_normal();
        entry.index = data.entries.size();

        // Parse the tags.
        while (skip_whitespace(contents), !contents.empty()) {
            // Skip to the next whitespace.
            auto end_of_tag = contents.find_first_of(whitespace);
            if (end_of_tag == std::string::npos) { end_of_tag = contents.length(); }

            // Add the tag found to the set of tags.
            Tag& tag = add_tag(data.tags, contents.substr(0, end_of_tag));
            contents.erase(0, std::min(contents.size(), end_of_tag + 1));

            // Keep track of entry within tag.
            tag.entries.insert(entry.index);

            // Keep track of tag within entry.
            entry.tags.insert(tag.index);
        }

        // Add the entry to entry_data vector.
        data.entries.push_back(std::move(entry));
    }
}

void add_directory_tagged_entries(TaggedEntries &data, const fs::path& dirpath, TaggedEntriesRecursion recurse) {
    fs::path dot_tag_file{""};
    // Loop over all files in the current directory.
    for (const auto& dir_it : fs::directory_iterator(dirpath)) {
        const auto& path = dir_it.path();
        if (is_directory(path)) {
            if (recurse == TaggedEntriesRecursion::Yes) {
                add_directory_tagged_entries(data, path, recurse);
            }
        } else if (is_regular_file(path)) {
            if (path.has_filename() && path.filename() == ".tag") {
                dot_tag_file = path;
                continue;
            }
            add_file_entry(data, path);
        }
    }

    // .tag file

    if (!dot_tag_file.empty()) {

        // TODO: Less code duplication from add_file_entry...

        static constexpr auto whitespace = "\r\n \t\v";
        static constexpr auto tag_marker = "#:";

        // Open .tag file
        File file{dot_tag_file};
        auto contents = file.read(1024);
        auto skip_whitespace = [&](std::string& contents) {
            auto pos = contents.find_first_not_of(whitespace);
            if (pos != std::string::npos) { contents.erase(0, pos); }
        };

        // Line-by-line, read filenames, ensure their existence, and parse tags.

        while (skip_whitespace(contents), !contents.empty()) {

            auto line = contents.substr(0, contents.size());
            auto end_of_line = line.find_first_of('\n');
            if (end_of_line != std::string::npos) {
                line.erase(end_of_line, line.length() - end_of_line);
            }

            skip_whitespace(line);
            if (line.empty()) { return; }

            auto end_of_entry_path = line.find_first_of(whitespace);
            if (end_of_entry_path == std::string::npos) { end_of_entry_path = line.size(); }

            Entry entry;
            entry.filepath = (fs::absolute(dirpath) / line.substr(0, end_of_entry_path)).lexically_normal();
            entry.index = data.entries.size();

            line.erase(0, end_of_entry_path);

            skip_whitespace(line);
            if (line.empty()) {
                fmt::print(".tag :: Expected \"{}\" tag marker, but got end of file.\n", tag_marker);
                return;
            }
            if (line.rfind(tag_marker, 0) != 0) {
                fmt::print(".tag :: Expected \"{}\" tag marker, but got something else entirely.\n", tag_marker);
                return;
            }

            line.erase(0, 2);

            while (skip_whitespace(line), !line.empty()) {
                // Skip to the next whitespace.
                auto end_of_tag = line.find_first_of(whitespace);
                if (end_of_tag == std::string::npos) { end_of_tag = line.size(); }
                // Add the tag found to the set of tags.
                Tag& tag = add_tag(data.tags, line.substr(0, end_of_tag));
                // Eat tag from beginning of string view.
                line.erase(0, std::min(line.size(), end_of_tag + 1));
                // Keep track of entry within tag.
                tag.entries.insert(entry.index);
                // Keep track of tag within entry.
                entry.tags.insert(tag.index);
            }
            // Add the entry to entry_data vector.
            data.entries.push_back(std::move(entry));

            contents.erase(0, end_of_line);
        }

    }

}

TaggedEntries get_directory_tagged_entries(const fs::path& dirpath, TaggedEntriesRecursion recurse) {
    TaggedEntries data;
    add_directory_tagged_entries(data, dirpath, recurse);
    return data;
}
