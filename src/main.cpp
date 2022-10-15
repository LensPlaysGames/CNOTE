#include <string.h>
#include <string_view>
#ifdef _WIN32
#  define __WXMSW__
#endif

#define _UNICODE

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include <filesystem>
#include <string>
#include <unordered_set>

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

    wxBoxSizer* layout;

    wxListBox *tag_list;
    wxArrayString tags;

    wxBoxSizer* entry_list;
    std::vector<wxButton*> entries;
};

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

Frame::Frame()
    : wxFrame(NULL, 0, "C-NOTE")
{
    SetFont(GetFont().Scale(1.5));

    // TODO: Create tags/entries data structure(s).

    // TODO: Maybe store these vectors in one struct?
    std::vector<Tag> tag_data;
    std::vector<Entry> entry_data;

    // TODO: Loop over all files in the current directory.
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

    // Populate entry_list.
    for (int i = 0; i < 10; ++i) {
        wxButton *button = new wxButton(this, i, wxString(std::to_string(i)));
        entry_list->Add(button, 1, 0);
    }

    layout->Add(tag_list, 1, wxEXPAND);
    layout->Add(entry_list);

    SetSizer(layout);
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
