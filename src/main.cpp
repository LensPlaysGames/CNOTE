#ifdef _WIN32
#  define __WXMSW__
#endif

#define _UNICODE

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include <filesystem>
#include <string.h>
#include <string>
#include <set>
#include <unordered_set>
#include <map>

struct Entry;

struct Tag {
    Tag(std::string tag_string);

    std::string tag;
    // Entries that contain this tag.
    std::unordered_set<size_t> entries;
    size_t index;
};

Tag::Tag(std::string tag_string) {
    tag = std::move(tag_string);
}

struct Entry {
    std::filesystem::path filepath;
    // Tags that contain this entry.
    std::unordered_set<size_t> tags;
    size_t index;
};

Tag* add_tag(std::vector<Tag>& tags, Tag& new_tag) {
    for (Tag& tag : tags) {
        if (tag.tag == new_tag.tag) {
            return &tag;
        }
    }
    new_tag.index = tags.size();
    tags.push_back(new_tag);
    return &tags.back();
}

class App : public wxApp {
public:
    virtual bool OnInit() override;
};

class Frame : public wxFrame {
public:
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
            std::cout << tag_index << " is shown and selected. " << tags.GetCount() << std::endl;
            tags_selected_list_index.insert(tags.GetCount());
        }
        tag_list_indices[tags.GetCount()] = tag_index;

        tags.Add(wxString(tag.tag.data()));
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
        wxButton *button = new wxButton(this, entry.index, wxString(entry.filepath.c_str()));
        entry_list->Add(button, 1, 0);
    }
    entry_list->Layout();
}

void Frame::OnTagSelectionChange(wxCommandEvent &event)
{
    std::cout << "\ntag int: " << event.GetInt() << "\n";

    std::cout << "\nmap:\n";
    try {

        for (const auto& [tag_list_index, tag_data_index] : tag_list_indices) {
            std::cout << "  (" << tag_list_index << ", " << tag_data_index << ")\n";
        }
        std::cout << std::flush;

        size_t tag_data_index = tag_list_indices.at(event.GetInt());

        std::cout << "\nnew int: " << event.GetInt() << "\n";
        std::cout << std::flush;

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
            bool has_all_tags = true;
            for (size_t tag_index : tags_selected) {
                const Tag& selected_tag = tag_data.at(tag_index);
                if (!selected_tag.entries.contains(entry.index)) {
                    has_all_tags = false;
                    break;
                }
            }
            if (has_all_tags) {
                // Make entry shown iff it has all selected tags.
                entries_shown.insert(entry.index);
            }
        }

        // Update tags shown based on set intersection of all shown entries' tags.
        tags_shown.clear();
        for (const Tag& tag : tag_data) {
            for (size_t entry_index : entries_shown) {
                const Entry& entry = entry_data.at(entry_index);
                if (entry.tags.contains(tag.index)) {
                    tags_shown.insert(tag.index);
                    break;
                }
            }
        }

        std::cout << "\nSelected:\n";
        for (size_t tag_index : tags_selected) {
            const Tag& tag = tag_data.at(tag_index);
            std::cout << tag.tag << " - " << tag.index << "\n";
        }

        //    std::cout << "\nShown:\n";
        //    for (size_t tag_index : tags_shown) {
        //        const Tag& tag = tag_data.at(tag_index);
        //        std::cout << tag.tag << " - " << tag.index << "\n";
        //    }
        //
        //    std::cout << "\nShown Entries:\n";
        //    for (size_t entry_index : entries_shown) {
        //        const Entry& entry = entry_data.at(entry_index);
        //        std::cout << entry.filepath << " - " << entry.index << "\n";
        //    }

        std::cout << std::flush;

        // Redisplay (remake frame basically).
        build_tag_gui();
        build_entry_gui();
        layout->Layout();

        std::cout << tag.tag << " - " << tag.index << std::endl;

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

}

void Frame::OnEntryClick(wxCommandEvent &event)
{
    const Entry& entry = entry_data.at(event.GetId());

    std::filesystem::path filepath = entry.filepath;
    if (!std::filesystem::is_directory(filepath)) {
        // Append "/.." to filepath to access parent directory.
        filepath = filepath.parent_path();
    }

    std::string command;

# if defined (_WIN32)
    command = "explorer.exe \"" + filepath.string() + "\"";
# elif defined (__APPLE__)
    command = "open '" + filepath.string() + "'";
# elif defined (__linux__)
    command = "xdg-open '" + filepath.string() + "'";
# else
#   error "Platform is not supported; can not implement file-opening feature."
# endif

    std::cout << command << std::endl;
    system(command.data());

    std::cout << entry.filepath << " - " << entry.index << std::endl;
}

Frame::Frame()
    : wxFrame(NULL, 0, "C-NOTE")
{
    SetFont(GetFont().Scale(1.5));

    // Loop over all files in the current directory.
    auto dir = std::filesystem::directory_iterator(".");
    for (const auto& dir_it : dir) {
        const auto& path = dir_it.path();
        if (is_directory(path)) {
            // TODO: Loop over all files in all subdirectories.
        } else if (is_regular_file(path)) {
            // Check for tags at beginning.

            //std::cout << path << " is a regular file." << std::endl;

            constexpr size_t contents_max_size = 1024;
            std::string contents;
            contents.reserve(contents_max_size);

            FILE* file = fopen(path.string().c_str(), "r");
            size_t bytes_read = fread(contents.data(), 1, contents_max_size, file);

            std::string whitespace("\r\n \t\v");
            std::string tag_marker("#:");

            char* tags_string = contents.data();

            // Skip whitespace at beginnning.
            size_t whitespace_at_beginning = strspn(tags_string, whitespace.data());
            tags_string += whitespace_at_beginning;

            if (memcmp(tags_string, tag_marker.data(), tag_marker.length()) == 0) {
                // Tags are at beginning.
                // Skip past the tag marker.
                tags_string += tag_marker.length();

                // Start to parse tags white-space separated, up until a newline is met.
                // Find end (first newline)
                char* end_of_tags = strstr(tags_string, "\n");
                // TODO: Handle the case of end of file, no newline.

                // Add new entry to entry_data vector.
                Entry new_entry;
                new_entry.filepath = path.string();
                entry_data.push_back(new_entry);
                Entry& entry = entry_data.back();
                entry.index = entry_data.size() - 1;

                while (tags_string < end_of_tags) {
                    // Skip past all whitespace.
                    size_t skip_amount = strspn(tags_string, whitespace.data());
                    if (skip_amount == 0) {
                        break;
                    }
                    tags_string += skip_amount;
                    char *tag_begin = tags_string;

                    // Skip to the next whitespace.
                    size_t tag_length = strcspn(tags_string, whitespace.data());
                    tags_string += tag_length;
                    char *tag_end = tags_string;

                    // Add the tag found to the set of tags.
                    Tag new_tag(std::string(tag_begin, tag_end));
                    Tag* tag = add_tag(tag_data, new_tag);

                    // Keep track of entry within tag.
                    tag->entries.insert(entry.index);

                    // Keep track of tag within entry.
                    entry.tags.insert(tag->index);
                }
            }
        }
    }

    std::cout << "Tags\n";
    for (const Tag& it : tag_data) {
        std::cout << it.tag << "\n";
        for (size_t entry_index : it.entries) {
            std::cout << "  "
                      << entry_data.at(entry_index).filepath.string().c_str()
                      << "\n";
        }
    }

    std::cout << "Entries\n";
    for (const auto& it : entry_data) {
        std::cout << it.filepath << "\n";
        for (size_t tag_index : it.tags) {
            std::cout << "  " << tag_data.at(tag_index).tag << "\n";
        }
    }
    std::cout << std::flush;


    // Create layout
    layout = new wxBoxSizer(wxVERTICAL);
    entry_list = new wxBoxSizer(wxVERTICAL);

    // Create widgets, fill in data.

    // Populate tag array.
    for (const Tag& it : tag_data) {
        tags.Add(wxString(it.tag.data()));
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
        wxButton *button = new wxButton(this, entry.index, wxString(entry.filepath.c_str()));
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
    Frame* frame = new Frame();
    frame->Show(true);
    return true;
}

void Frame::OnExit(wxCommandEvent& event) {
    (void)event;
    Close(true);
}

wxIMPLEMENT_APP(App);
