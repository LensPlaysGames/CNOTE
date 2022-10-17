#include <string_view>
#include <unordered_set>
#include <filesystem>

namespace fs = std::filesystem;

struct Tag {
    Tag(std::string_view tag_string);

    std::string tag;
    // Entries that contain this tag.
    std::unordered_set<size_t> entries;
    size_t index = 0;
};

Tag& add_tag(std::vector<Tag>& tags, Tag&& new_tag);

struct Entry {
    fs::path filepath;
    // Tags that contain this entry.
    std::unordered_set<size_t> tags;
    size_t index = 0;
};

struct TaggedEntries {
    // Tag data
    std::vector<Tag> tags;
    std::unordered_set<size_t> tags_shown;
    std::unordered_set<size_t> tags_selected;
    // Entry data
    std::vector<Entry> entries;
    std::unordered_set<size_t> entries_shown;
};
