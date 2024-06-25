#include <cnote/cnote.h>

#include <cstdio>
#include <string>
#include <string_view>

namespace cnote {
void skip_whitespace(std::string_view& s) {
    auto past = s.find_first_not_of(whitespace);
    if (past != std::string::npos) s.remove_prefix(past);
}

void skip_comments(std::string_view& s) {
    skip_whitespace(s);

    // C-style comments
    if (s.starts_with("//") or s.starts_with("/*")) {
        s.remove_prefix(2);
        skip_whitespace(s);
        while (s[0] == '/' or s[0] == '*') {
            s.remove_prefix(1);
            skip_whitespace(s);
        }
    }

    // LISP comments
    while (s.starts_with(";")) {
        s.remove_prefix(1);
        skip_whitespace(s);
    }

    // LaTeX comments
    while (s.starts_with("%")) {
        s.remove_prefix(1);
        skip_whitespace(s);
    }
}

std::vector<std::string> parse_line(std::string_view& line) {
    skip_whitespace(line);
    if (line.empty()) {
        // Empty line, definitely no tags.
        return {};
    }

    if (not(line.starts_with(tag_marker))) {
        // If line, even past whitespace and comment starters, doesn't have a
        // tag marker, then this line doesn't have tags on it. Done parsing, no
        // tags.
        return {};
    }
    line.remove_prefix(tag_marker.size());

    // Parse the tags.
    std::vector<std::string> out{};
    while (skip_whitespace(line), line.size()) {
        // Skip to the next whitespace.
        auto end_of_tag = line.find_first_of(whitespace);
        if (end_of_tag == std::string::npos) end_of_tag = line.size();
        // Add the tag found to the set of tags.
        auto tag = std::string(line.substr(0, end_of_tag));
        out.push_back(tag);
        line.remove_prefix(end_of_tag);
    }
    return out;
}

void Entry::MaybeCreate(Context& context, const std::string path) {
    auto f = fopen(path.data(), "rb");
    if (not f) {
        std::printf(
            "Entry::Create(): Could not open file at %s\n", path.data()
        );
        return;
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
    if (nread != size_t(read_amount)) {
        fclose(f);
        std::printf(
            "Entry::Create(): Trouble reading file at %s (expected %ld bytes, "
            "got %zu)\n",
            path.data(), read_amount, nread
        );
        return;
    }

    fclose(f);

    auto entry = Entry(path, {});
    auto entry_ref = context.entries.size();

    auto handle_tag = [&](const std::string& tag) {
        if (tag.empty()) return;
        // Make both Tag and Entry to store in Context, and somehow make the
        // Entry reference the Tag and the Tag reference the Entry.
        auto tag_ref = context.register_tag(tag);
        entry.tags.push_back(tag_ref);
        context.tags.at(tag_ref).entries.push_back(entry_ref);
    };

    auto end_of_line = contents.find_first_of('\n');
    auto line = std::string_view(
        contents.begin(),
        end_of_line == std::string::npos ? contents.end()
                                         : contents.begin() + end_of_line
    );
    auto tags = parse_line(line);
    for (auto tag : tags) handle_tag(tag);

    // If no tags were found AND the first line actually ended (implying
    // existence of a second line), check the second line.
    if (tags.empty() and end_of_line != std::string::npos) {
        auto end_of_line2 = contents.find_first_of('\n', end_of_line + 1);
        auto line2 = std::string_view(
            contents.begin() + end_of_line + 1,
            end_of_line2 == std::string::npos ? contents.end()
                                              : contents.begin() + end_of_line2
        );
        auto tags2 = parse_line(line2);
        for (auto tag : tags2) handle_tag(tag);
    }

    if (entry.tags.size()) context.entries.push_back(entry);
}
}  // namespace cnote
