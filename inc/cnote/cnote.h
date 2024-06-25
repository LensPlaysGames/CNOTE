#ifndef CNOTE_H
#define CNOTE_H

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace cnote {

static constexpr auto bytes_to_read_when_looking_for_tags = 512;
static constexpr std::string_view tag_marker = "#:";
static constexpr auto whitespace = "\r\n \t\v";

struct Context;
struct Entry;

// Basically, Tags and Entries both need to map to their respective
// Entries and Tags; each Tag has a one-to-many relationship with Entries,
// and each Entry has a one-to-many relationship with Tags.
struct Tag {
    std::string text;
    std::vector<std::size_t> entries;

    bool operator==(const std::string_view other) const {
        return text == other;
    };
    bool operator==(const Tag& other) const { return operator==(other.text); };
};

// Split into whitespace separated words and return a list of unique words.
std::vector<std::string> parse_tags(std::string_view& some_tags);
// Skips whitespace and common comment-starting characters at beginning of
// line, then calls parse_tags() iff tag_marker appears.
std::vector<std::string> parse_line(std::string_view& line);

struct Entry {
    const std::filesystem::path filepath;
    std::vector<std::size_t> tags;

    Entry(std::filesystem::path path)
        : filepath(path.make_preferred().lexically_normal()), tags({}) {}
};

/// To use this context, the idea is this:
/// - Fetch an Entry
/// - Call register_tag() for every tag found on the entry; this allows
///   finding entries by tag vs just tag by entry.
struct Context {
    // The idea is that the filesystem will be read from, and the entire
    // entries list is invalidated and replaced with fresh data, utilising
    // and/or updating the Tag Cache to keep references to tags in every
    // entry. Or something like that.
    std::vector<Entry> entries;
    // unordered_set is nice because it ensures that, "References and pointers
    // to data stored in the container are only invalidated by erasing that
    // element, even when the corresponding iterator is invalidated."
    std::vector<Tag> tags;

    std::size_t register_tag(std::string tag) {
        auto found = std::find(tags.begin(), tags.end(), tag);
        // Push new tag
        if (found == tags.end()) {
            tags.push_back({std::move(tag), {}});
            return tags.size() - 1;
        }
        // Return tag already in cache
        return found - tags.begin();
    }

    std::size_t register_entry(const std::filesystem::path filepath) {
        auto found = std::find_if(
            entries.begin(), entries.end(),
            [&](const Entry& candidate) {
                return candidate.filepath == filepath;
            }
        );
        if (found == entries.end()) {
            entries.push_back(Entry{filepath});
            return entries.size() - 1;
        }
        return found - entries.begin();
    }

    std::size_t tagfile(std::string dirpath, std::vector<std::string_view> filter_tags = {});

    std::size_t traverse_file(
        const std::string_view path,
        std::vector<std::string_view> filter_tags = {}
    );
};

}  // namespace cnote

#endif /* CNOTE_H */
