#include <cnote/cnote.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>

namespace cnote {
bool skip_whitespace(std::string_view& s) {
    auto past = s.find_first_not_of(whitespace);

    // If we are not at whitespace, nothing to skip.
    if (not past) return false;

    if (past == std::string::npos) s.remove_prefix(s.size());
    else
        s.remove_prefix(past);
    return true;
}

bool skip_comments(std::string_view& s) {
    bool changed{false};
    changed = skip_whitespace(s);

    if (s.starts_with(tag_marker)) return changed;

    // C-style comments
    if (s.starts_with("//") or s.starts_with("/*")) {
        changed = true;
        s.remove_prefix(2);
        skip_whitespace(s);
        while (s[0] == '/' or s[0] == '*') {
            changed = true;
            s.remove_prefix(1);
            skip_whitespace(s);
        }
    }

    // LISP comments
    while (s.starts_with(";") and not s.starts_with(tag_marker)) {
        changed = true;
        s.remove_prefix(1);
        skip_whitespace(s);
    }

    // LaTeX comments
    while (s.starts_with("%") and not s.starts_with(tag_marker)) {
        changed = true;
        s.remove_prefix(1);
        skip_whitespace(s);
    }

    while (s.starts_with("#") and not s.starts_with(tag_marker)) {
        changed = true;
        s.remove_prefix(1);
        skip_whitespace(s);
    }

    return changed;
}

std::vector<std::string> parse_tags(std::string_view& some_tags) {
    std::vector<std::string> out{};
    while (skip_whitespace(some_tags), some_tags.size()) {
        /// LEX TAG
        // Skip to the next whitespace.
        auto end_of_tag = some_tags.find_first_of(whitespace);
        if (end_of_tag == std::string::npos) end_of_tag = some_tags.size();
        auto tag_view = some_tags.substr(0, end_of_tag);

        /// DE-DUPLICATE
        // Ensure we don't add any duplicate tags.
        auto duplicate = std::find(out.begin(), out.end(), tag_view);
        if (duplicate == out.end()) {
            // Add the tag found to the set of tags.
            out.push_back(std::string(tag_view));
        }

        /// ADVANCE
        some_tags.remove_prefix(end_of_tag);
    }
    return out;
}

std::vector<std::string> parse_line(std::string_view& line) {
    // If either of comment starters or whitespace appears, we will skip it
    // and then look for either to appear again, skipping that.
    while (line.size() and (skip_whitespace(line) or skip_comments(line)))
        ;
    // Empty line, definitely no tags.
    if (line.empty()) return {};

    // If line, even past whitespace and comment starters, doesn't have a
    // tag marker, then this line doesn't have tags on it. Done parsing, no
    // tags.
    if (not(line.starts_with(tag_marker))) return {};

    line.remove_prefix(tag_marker.size());

    // Parse the tags.
    return parse_tags(line);
}

std::size_t Context::traverse_file(
    const std::string_view path,
    std::vector<std::string_view> filter_tags
) {
    constexpr auto noentry{(std::size_t)-1};

    auto f = fopen(path.data(), "rb");
    if (not f) {
        std::printf(
            "Context::register_entry(): Could not open file at %s\n",
            path.data()
        );
        return noentry;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    auto fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    auto read_amount = fsize > bytes_to_read_when_looking_for_tags
        ? bytes_to_read_when_looking_for_tags
        : fsize;

    // Allocate memory for file
    std::string contents{};
    contents.resize(read_amount);

    // Read file into allocated memory
    auto nread = fread(contents.data(), 1, read_amount, f);
    fclose(f);
    if (nread != size_t(read_amount)) {
        std::printf(
            "Context::register_entry(): Trouble reading file at %s (expected "
            "%ld bytes, got %zu)\n",
            path.data(), read_amount, nread
        );
        return noentry;
    }

    // NOTE: It's quite important past this register_entry() call to remove
    // the entry in entries at entry_ref if for some reason this entry should
    // not end up being added to the Context.
    auto entry_ref{std::size_t(-1)};
    Entry* entry = nullptr;

    auto handle_tag = [&](const std::string& tag) {
        if (tag.empty()) return;

        // We do this here so that we only register an entry iff we find a tag
        // on it.
        if (entry_ref == std::size_t(-1)) {
            entry_ref = register_entry(path);
            entry = &entries.at(entry_ref);
        }

        // Make both Tag and Entry to store in Context, and somehow make the
        // Entry reference the Tag and the Tag reference the Entry.
        auto tag_ref = register_tag(tag);
        entry->tags.push_back(tag_ref);
        tags.at(tag_ref).entries.push_back(entry_ref);
    };

    auto end_of_line = contents.find_first_of('\n');
    auto line = std::string_view(
        contents.begin(),
        end_of_line == std::string::npos ? contents.end()
                                         : contents.begin() + end_of_line
    );
    auto parsed_tags = parse_line(line);
    for (auto tag : parsed_tags) handle_tag(tag);

    // If no tags were found AND the first line actually ended (implying
    // existence of a second line), check the second line.
    if (parsed_tags.empty() and end_of_line != std::string::npos) {
        auto end_of_line2 = contents.find_first_of('\n', end_of_line + 1);
        auto line2 = std::string_view(
            contents.begin() + end_of_line + 1,
            end_of_line2 == std::string::npos ? contents.end()
                                              : contents.begin() + end_of_line2
        );
        auto parsed_tags2 = parse_line(line2);
        for (auto tag : parsed_tags2) handle_tag(tag);
    }

    // If no tags were found at all, this is not an entry we need to record.
    if (not entry or entry->tags.empty()) return noentry;

    bool passed_at_least_one_filter{true};
    if (filter_tags.size()) {
        passed_at_least_one_filter = false;
        for (auto filter_tag : filter_tags) {
            if (std::find_if(
                    entry->tags.begin(), entry->tags.end(),
                    [&](auto candidate_i) {
                        return tags.at(candidate_i) == filter_tag;
                    }
                )
                != entry->tags.end())
                passed_at_least_one_filter = true;
        }
    }
    if (not passed_at_least_one_filter) {
        entries.pop_back();
        return noentry;
    }

    return entry_ref;
}

std::size_t Context::tagfile(
    std::string dirpath,
    std::vector<std::string_view> filter_tags
) {
    if (not dirpath.ends_with('/')) dirpath += '/';
    dirpath += ".tag";
    auto f = fopen(dirpath.data(), "rb");
    if (not f) {
        // std::printf("No tagfile at %s\n", dirpath.data());
        return {};
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    auto fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Allocate memory for file
    std::string contents{};
    contents.resize(fsize);

    // Read file into allocated memory
    auto nread = fread(contents.data(), 1, fsize, f);
    fclose(f);
    if (nread != size_t(fsize)) {
        std::printf(
            "Trouble reading file at %s (expected %ld bytes, got %zu)\n",
            dirpath.data(), fsize, nread
        );
        return {};
    }

    std::string_view view{contents};

    while (skip_whitespace(view), view.size()) {
        auto end_of_line = view.find_first_of('\n');
        if (end_of_line == std::string::npos) end_of_line = view.size();

        auto line = view.substr(0, end_of_line);
        view.remove_prefix(end_of_line);

        skip_whitespace(line);
        // TODO: Support quoted filenames for filenames with spaces.
        auto end_of_path = line.find_first_of(whitespace);
        auto path = line.substr(0, end_of_path);
        line.remove_prefix(end_of_path);

        skip_whitespace(line);
        if (not line.starts_with(tag_marker)) {
            std::printf(
                "Expected tag marker after filepath %s in tagfile at %s\n",
                path.data(), dirpath.data()
            );
            // TODO: Could try to just skip to next line instead of returning.
            return {};
        }
        // Eat tag marker
        line.remove_prefix(tag_marker.size());

        // Parse tags
        auto parsed_tags = parse_tags(line);

        bool passed_at_least_one_filter{true};
        if (filter_tags.size()) {
            passed_at_least_one_filter = false;
            for (auto filter_tag : filter_tags) {
                if (std::find(
                        parsed_tags.begin(), parsed_tags.end(), filter_tag
                    )
                    != parsed_tags.end())
                    passed_at_least_one_filter = true;
            }
        }
        if (not passed_at_least_one_filter) return {};

        auto entry_i = register_entry(path);
        auto& entry = entries.at(entry_i);

        for (auto tag : parsed_tags) {
            auto tag_ref = register_tag(tag);
            entry.tags.push_back(tag_ref);
            tags.at(tag_ref).entries.push_back(entry_i);
        }
    }

    return {};
}
}  // namespace cnote
