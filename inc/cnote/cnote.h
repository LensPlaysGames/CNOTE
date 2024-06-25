#ifndef CNOTE_H
#define CNOTE_H

#include <algorithm>
#include <string>
#include <vector>

namespace cnote {

constexpr auto bytes_to_read_when_looking_for_tags = 512;
constexpr std::string_view tag_marker = "#:";
static constexpr auto whitespace = "\r\n \t\v";

struct Context;
struct Entry;

// Basically, Tags and Entries both need to map to their respective
// Entries and Tags; each Tag has a one-to-many relationship with Entries,
// and each Entry has a one-to-many relationship with Tags.
struct Tag {
    std::string text;
    std::vector<std::size_t> entries;

    bool operator==(const std::string_view other) const { return text == other; };
    bool operator==(const Tag& other) const { return operator==(other.text); };
};

struct Entry {
    const std::string filepath;
    std::vector<std::size_t> tags;
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

    std::size_t register_entry(Entry entry) {
        entries.push_back(entry);
        return entries.size() - 1;
    }
    std::size_t register_entry(
        const std::string_view path,
        std::vector<std::string_view> filter_tags = {}
    );
};

}  // namespace cnote

#endif /* CNOTE_H */
