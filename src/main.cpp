#ifdef _WIN32
#  define __WXMSW__
#endif

#ifndef _UNICODE
#  define _UNICODE
#endif

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include <fmt/format.h>

#include <filesystem>
#include <cstring>
#include <string>
#include <set>
#include <unordered_set>
#include <map>
#include <ranges>

namespace ranges = std::ranges;
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

struct Tag {
    Tag(std::string_view tag_string);

    std::string tag;
    // Entries that contain this tag.
    std::unordered_set<size_t> entries;
    size_t index = 0;
};

Tag::Tag(std::string_view tag_string): tag(tag_string) { }

struct Entry {
    fs::path filepath;
    // Tags that contain this entry.
    std::unordered_set<size_t> tags;
    size_t index = 0;
};

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

struct App : public wxApp {
    virtual bool OnInit() override;
};

struct Frame : public wxFrame {
    Frame();
private:
    void OnEntryClick(wxCommandEvent &event);
    void OnTagSelectionChange(wxCommandEvent &event);
    void OnExit(wxCommandEvent& event);

    std::vector<Tag> tag_data;
    std::unordered_set<size_t> tags_shown;
    std::unordered_set<size_t> tags_selected;
    void build_tag_gui();
    wxListBox *tag_list;
    wxArrayString tags;

    std::vector<Entry> entry_data;
    std::unordered_set<size_t> entries_shown;
    void build_entry_gui();

    // Index within tag_list -> Index within tag_data
    std::map<size_t, size_t> tag_list_indices;

    wxBoxSizer* layout;
    wxBoxSizer* entry_list;
};

void Frame::build_tag_gui() {
    // TODO: Repurpose existing elements (change string in-place)
    // without having to clear and re-create entire tags array and tag
    // list each time.
    std::unordered_set<size_t> tags_selected_list_index;
    tags.Clear();
    tag_list_indices.clear();
    for (size_t tag_index : tags_shown) {
        Tag& tag = tag_data.at(tag_index);
        if (tags_selected.contains(tag_index)) {
            fmt::print("{} is shown and selected. {}\n", tag_index, tags.GetCount());
            tags_selected_list_index.insert(tags.GetCount());
        }
        tag_list_indices[tags.GetCount()] = tag_index;

        tags.Add(tag.tag);
    }
    // Create new tag list from tags array.
    tag_list->Set(tags);
    for (size_t selected_tag_list_index : tags_selected_list_index) {
        tag_list->SetSelection(selected_tag_list_index);
    }
}

void Frame::build_entry_gui() {
    // TODO: Repurpose existing buttons, only changing strings
    // in-place, to reduce time spent destroying and re-building
    // buttons.
    entry_list->Clear(true);
    for (size_t entry_index : entries_shown) {
        const Entry& entry = entry_data.at(entry_index);
        auto* button = new wxButton(this, entry.index, entry.filepath.string());
        entry_list->Add(button, 1, 0);
    }
    entry_list->Layout();
}

void Frame::OnTagSelectionChange(wxCommandEvent &event)
{
    fmt::print("\ntag int: {}\n", event.GetInt());

    fmt::print("\nmap:\n");
    try {

        for (const auto& [tag_list_index, tag_data_index] : tag_list_indices) {
            fmt::print("  ({},{})\n", tag_list_index, tag_data_index);
        }

        size_t tag_data_index = tag_list_indices.at(event.GetInt());

        fmt::print("\nnew int: {}\n", event.GetInt());

        const Tag& tag = tag_data.at(tag_data_index);

        if (event.IsSelection()) {
            // Selected.
            // Add tag to selected tags.
            tags_selected.insert(tag.index);
        } else {
            // Deselected.
            // Remove tag from selected tags.
            tags_selected.erase(tag.index);
        }

        // Update entries shown based on set intersection of all selected tags' entries.
        entries_shown.clear();
        for (const Entry& entry : entry_data) {
            // If entry is contained in *all* selected tags, make entry shown.
            bool has_all_tags = ranges::all_of(tags_selected,
                [&](size_t tag_index){ return tag_data.at(tag_index).entries.contains(entry.index); });
            if (has_all_tags) { entries_shown.insert(entry.index); }
        }

        // Update tags shown based on set intersection of all shown entries' tags.
        tags_shown.clear();
        for (const Tag& tag : tag_data) {
            auto used = ranges::any_of(entries_shown,
                [&](size_t entry_index){ return entry_data.at(entry_index).tags.contains(tag.index); });
            if (used) { tags_shown.insert(tag.index); }
        }

        fmt::print("\nSelected:\n");
        for (size_t tag_index : tags_selected) {
            const Tag& tag = tag_data.at(tag_index);
            fmt::print("{} - {}\n", tag.tag, tag.index);
        }

        //    fmt::print("\nShown:\n");
        //    for (size_t tag_index : tags_shown) {
        //        const Tag& tag = tag_data.at(tag_index);
        //        fmt::print("{} - {}\n", tag.tag, tag.index);
        //    }
        //
        //    fmt::print("\nShown Entries:\n");
        //    for (size_t entry_index : entries_shown) {
        //        const Entry& entry = entry_data.at(entry_index);
        //        fmt::print("{} - {}\n", entry.filepath.string(), entry.index);
        //    }

        // Redisplay (remake frame basically).
        build_tag_gui();
        build_entry_gui();
        layout->Layout();

        fmt::print("{} - {}\n", tag.tag, tag.index);

    } catch (const std::exception& e) {
        fmt::print("{}\n", e.what());
    }

}

void Frame::OnEntryClick(wxCommandEvent &event)
{
    const Entry& entry = entry_data.at(event.GetId());
    const auto filepath = is_directory(entry.filepath) ? entry.filepath : entry.filepath.parent_path();

# if defined (_WIN32)
    static constexpr auto fmt_str = "explorer.exe \"{}\"";
# elif defined (__APPLE__)
    static constexpr auto fmt_str = "open '{}'";
# elif defined (__linux__)
    static constexpr auto fmt_str = "xdg-open '{}'";
# else
#   error "Platform is not supported; can not implement file-opening feature."
# endif

    const auto command = fmt::format(fmt_str, filepath.string());
    fmt::print("{}\n", command);
    system(command.data());

    fmt::print("{} - {}\n", entry.filepath.string(), entry.index);
}

Frame::Frame()
    : wxFrame(nullptr, 0, "C-NOTE")
{
    Frame::SetFont(GetFont().Scale(1.5));

    // Loop over all files in the current directory.
    for (const auto& dir_it : fs::directory_iterator(".")) {
        const auto& path = dir_it.path();
        if (is_directory(path)) {
            // TODO: Loop over all files in all subdirectories.
        } else if (is_regular_file(path)) {
            // Check for tags at beginning.

            //fmt::print("{} is a regular file.\n", path.string());
            static constexpr size_t contents_max_size = 1024;
            static constexpr auto whitespace = "\r\n \t\v"sv;
            static constexpr auto tag_marker = "#:"sv;

            File file{path};
            auto contents = file.read(contents_max_size);

            auto sv = std::string_view{contents};
            auto skip_whitespace = [&](std::string_view& sv) {
                auto pos = sv.find_first_not_of(whitespace);
                if (pos != std::string_view::npos) { sv.remove_prefix(pos); }
            };

            // Skip whitespace at beginnning.
            skip_whitespace(sv);
            if (sv.empty()) { continue; } // String is entirely whitespace.

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
                entry.index = entry_data.size();

                // Parse the tags.
                while (skip_whitespace(sv), !sv.empty()) {
                    // Skip to the next whitespace.
                    auto end_of_tag = sv.find_first_of(whitespace);
                    if (end_of_tag == std::string::npos) { end_of_tag = sv.size(); }

                    // Add the tag found to the set of tags.
                    Tag& tag = add_tag(tag_data, sv.substr(0, end_of_tag));
                    sv.remove_prefix(std::min(sv.size(), end_of_tag + 1));

                    // Keep track of entry within tag.
                    tag.entries.insert(entry.index);

                    // Keep track of tag within entry.
                    entry.tags.insert(tag.index);
                }

                // Add the entry to entry_data vector.
                entry_data.push_back(std::move(entry));
            }
        }
    }

    fmt::print("Tags\n");
    for (const Tag& it : tag_data) {
        fmt::print("{}\n", it.tag);
        for (size_t entry_index : it.entries) {
            fmt::print("  {}\n", entry_data.at(entry_index).filepath.string());
        }
    }

    fmt::print("Entries\n");
    for (const auto& it : entry_data) {
        fmt::print("{}\n", it.filepath.string());
        for (size_t tag_index : it.tags) {
            fmt::print("  {}\n", tag_data.at(tag_index).tag);
        }
    }


    // Create layout
    layout = new wxBoxSizer(wxVERTICAL);
    entry_list = new wxBoxSizer(wxVERTICAL);

    // Create widgets, fill in data.

    // Populate tag array.
    for (const Tag& it : tag_data) {
        tags.Add(it.tag.data());
    }

    // Create new tag_list from tag array.
    tag_list = new wxListBox
        (this, 0,
         wxDefaultPosition, wxDefaultSize,
         tags.Count(), tags.begin(),
         wxLB_MULTIPLE);

    for (size_t i = 0; i < tag_list->GetCount(); ++i) {
        tag_list_indices[i] = i;
    }

    // Populate entry_list.
    for (const Entry& entry : entry_data) {
        auto* button = new wxButton(this, entry.index, entry.filepath.string());
        entry_list->Add(button, 1, 0);
    }

    layout->Add(tag_list, 1, wxEXPAND);
    layout->Add(entry_list);

    layout->Layout();
    SetSizer(layout);

    Bind(wxEVT_LISTBOX, &Frame::OnTagSelectionChange, this);
    Bind(wxEVT_BUTTON, &Frame::OnEntryClick, this);
}

bool App::OnInit() {
    // Do NOT delete this. frame->Show() makes it so that the library takes care
    // of delete’ing the frame when appropriate.
    auto* frame = new Frame();
    frame->Show(true);
    return true;
}

void Frame::OnExit(wxCommandEvent&) {
    Close(true);
}

wxIMPLEMENT_APP(App);
