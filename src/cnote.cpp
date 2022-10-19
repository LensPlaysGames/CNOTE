#include "cnote.h"

#include <fmt/format.h>

#include <string_view>
#include <unordered_set>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;
using namespace std::string_literals;
using namespace std::string_view_literals;


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
    explicit File(std::same_as<fs::path> auto path, std::string_view mode = "r") : File(path.string(), mode) {}
    explicit File(std::string_view path, std::string_view mode = "r") { open(path, mode); }

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
    void open(std::string_view path, std::string_view mode) {
        close();
        file = fopen(path.data(), mode.data());
        if (!file) { /* TODO: handle error */ }

        // Set file modes.
        readable = mode.find('r') != std::string_view::npos;
        writable = mode.find_first_of("wa") != std::string_view::npos;
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


Tag::Tag(std::string_view tag_string): tag(tag_string) { }

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
    static constexpr auto whitespace = "\r\n \t\v"sv;
    static constexpr auto tag_marker = "#:"sv;

    File file{path};
    auto contents = file.read(1024);

    auto sv = std::string_view{contents};
    auto skip_whitespace = [&](std::string_view& sv) {
        auto pos = sv.find_first_not_of(whitespace);
        if (pos != std::string_view::npos) { sv.remove_prefix(pos); }
    };

    // Skip whitespace at beginnning.
    skip_whitespace(sv);
    // If file has nothing but whitespace, it is not an entry; skip it.
    if (sv.empty()) { return; }

    // Skip C-style comments.
    if (sv.starts_with("//") || sv.starts_with("/*")) {
        sv.remove_prefix(2);
        // Skip to next whitespace, then begin tag parsing.
        skip_whitespace(sv);
        // If file has nothing but whitespace, it is not an entry; skip it.
        if (sv.empty()) { return; }
    }
    // Skip latex style comments.
    else if (sv.starts_with("%")) {
        sv.remove_prefix(1);
        skip_whitespace(sv);
        if (sv.empty()) { return; }
    }
    // Skip LISP style comments.
    else if (sv.starts_with(";")) {
        while (sv.starts_with(";")) {
            sv.remove_prefix(1);
            // If file has nothing but whitespace, it is not an entry; skip it.
            if (sv.empty()) { return; }
        }
        skip_whitespace(sv);
        if (sv.empty()) { return; }
    }

    if (sv.starts_with(tag_marker)) {
        // Tags are at beginning.
        // Skip past the tag marker.
        sv.remove_prefix(tag_marker.size());

        // Start to parse tags white-space separated, up until a newline is met.
        // Find end (first newline)
        auto end_of_tags = sv.find_first_of('\n');
        if (end_of_tags != std::string::npos) {
            sv.remove_suffix(sv.size() - end_of_tags);
        }

        // Create a new entry.
        Entry entry;
        entry.filepath = path;
        entry.index = data.entries.size();

        // Parse the tags.
        while (skip_whitespace(sv), !sv.empty()) {
            // Skip to the next whitespace.
            auto end_of_tag = sv.find_first_of(whitespace);
            if (end_of_tag == std::string::npos) { end_of_tag = sv.size(); }

            // Add the tag found to the set of tags.
            Tag& tag = add_tag(data.tags, sv.substr(0, end_of_tag));
            sv.remove_prefix(std::min(sv.size(), end_of_tag + 1));

            // Keep track of entry within tag.
            tag.entries.insert(entry.index);

            // Keep track of tag within entry.
            entry.tags.insert(tag.index);
        }

        // Add the entry to entry_data vector.
        data.entries.push_back(std::move(entry));
    }
}

TaggedEntries get_directory_tagged_entries(const fs::path& path, TaggedEntriesRecursion recurse) {
    TaggedEntries data;
    fs::path dot_tag_file{""};
    // Loop over all files in the current directory.
    for (const auto& dir_it : fs::directory_iterator(path)) {
        const auto& path = dir_it.path();
        if (is_directory(path)) {
            if (recurse == TaggedEntriesRecursion::Yes) {
                // TODO: Loop over all files in all subdirectories.
            }
        } else if (is_regular_file(path)) {
            if (path.has_filename() && path.filename() == ".tag") {
                dot_tag_file = path;
            }
            add_file_entry(data, path);
        }
    }

    // .tag file

    if (!dot_tag_file.empty()) {

        // TODO: Less code duplication from add_file_entry...
        static constexpr auto whitespace = "\r\n \t\v"sv;
        static constexpr auto tag_marker = "#:"sv;

        // Open .tag file
        File file{dot_tag_file};
        auto contents = file.read(1024);
        auto sv = std::string_view{contents};
        auto skip_whitespace = [&](std::string_view& sv) {
            auto pos = sv.find_first_not_of(whitespace);
            if (pos != std::string_view::npos) { sv.remove_prefix(pos); }
        };

        // Line-by-line, read filenames, ensure their existence, and parse tags.

        while (skip_whitespace(sv), !sv.empty()) {

            auto line = sv.substr(0, sv.size());
            auto end_of_line = sv.find_first_of('\n');
            if (end_of_line != std::string::npos) {
                line.remove_suffix(line.size() - end_of_line);
            }

            skip_whitespace(line);
            if (line.empty()) { return data; }

            auto end_of_entry_path = line.find_first_of(whitespace);
            if (end_of_entry_path == std::string::npos) { end_of_entry_path = line.size(); }

            Entry entry;
            entry.filepath = line.substr(0, end_of_entry_path);
            entry.index = data.entries.size();

            line.remove_prefix(end_of_entry_path);

            skip_whitespace(line);
            if (line.empty()) {
                fmt::print(".tag :: Expected \"{}\" tag marker, but got end of file.\n", tag_marker);
                return data;
            }
            if (!line.starts_with(tag_marker)) {
                fmt::print(".tag :: Expected \"{}\" tag marker, but got something else entirely.\n", tag_marker);
                return data;
            }

            line.remove_prefix(tag_marker.size());

            while (skip_whitespace(line), !line.empty()) {
                // Skip to the next whitespace.
                auto end_of_tag = line.find_first_of(whitespace);
                if (end_of_tag == std::string::npos) { end_of_tag = line.size(); }
                // Add the tag found to the set of tags.
                Tag& tag = add_tag(data.tags, line.substr(0, end_of_tag));
                // Eat tag from beginning of string view.
                line.remove_prefix(std::min(line.size(), end_of_tag + 1));
                // Keep track of entry within tag.
                tag.entries.insert(entry.index);
                // Keep track of tag within entry.
                entry.tags.insert(tag.index);
            }
            // Add the entry to entry_data vector.
            data.entries.push_back(std::move(entry));

            sv.remove_prefix(end_of_line);
        }

    }
    return data;
}
