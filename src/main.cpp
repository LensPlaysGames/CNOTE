#if defined (_WIN32) && !defined (__WXMSW__)
#  define __WXMSW__
#endif

#ifndef _UNICODE
#  define _UNICODE
#endif

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#  include <wx/wx.h>
#  include <wx/filepicker.h>
#  include <wx/wrapsizer.h>
#endif

#include <fmt/format.h>

#include <filesystem>
#include <cstring>
#include <string>
#include <set>
#include <unordered_set>
#include <map>
#include <ranges>
#include <vector>

#include "cnote.h"

namespace ranges = std::ranges;
namespace fs = std::filesystem;
using namespace std::string_literals;
using namespace std::string_view_literals;

struct App : public wxApp {
    virtual bool OnInit() override;
};

struct Frame : public wxFrame {
    Frame(std::string dir_path);
private:
    void OnDirectoryChange(wxCommandEvent& event);
    void OnEntryClick(wxCommandEvent& event);
    void OnTagSelectionChange(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
    void OnResetTagSelection(wxCommandEvent& event);
    void OpenDirectoryDialog(wxCommandEvent& event);

    wxDirPickerCtrl* dir_picker;

    TaggedEntries data;
    void get_data(std::string dir_path);

    void build_tag_gui();
    wxListBox *tag_list;
    wxArrayString tag_string_array;

    void build_entry_gui();

    // Index within tag_list -> Index within tag_data
    std::map<size_t, size_t> tag_list_indices;

    wxBoxSizer* layout;
    wxStaticBoxSizer* tags_sizer;
    wxStaticBoxSizer* entry_sizer;

    wxWrapSizer* entry_list;

    void create_menubar();

    void refresh_shown_tags();
    void refresh_shown_entries();
    void refresh_shown();

    void redisplay();

    void get_data_and_refresh_gui(std::string dir_path);
};

void Frame::get_data_and_refresh_gui(std::string dir_path) {
    if (fs::is_directory(dir_path)) {
        get_data(dir_path);
        refresh_shown();
        redisplay();
    }
}

void Frame::redisplay() {
    build_tag_gui();
    build_entry_gui();
    layout->Layout();
}

void Frame::refresh_shown_tags() {
    data.tags_shown.clear();
    for (const Tag& tag : data.tags) {
        // If tag is contained in any one of the shown entries, make tag shown.
        auto used = ranges::any_of(data.entries_shown,
                                   [&](size_t entry_index){ return data.entries.at(entry_index).tags.contains(tag.index); });
        if (used) { data.tags_shown.insert(tag.index); }
    }
}

void Frame::refresh_shown_entries() {
    data.entries_shown.clear();
    for (const Entry& entry : data.entries) {
        // If entry is contained in *all* selected tags, make entry shown.
        bool has_all_tags = ranges::all_of(data.tags_selected,
                                           [&](size_t tag_index){ return data.tags.at(tag_index).entries.contains(entry.index); });
        if (has_all_tags) { data.entries_shown.insert(entry.index); }
    }
}

void Frame::refresh_shown() {
    refresh_shown_entries();
    refresh_shown_tags();
}

void Frame::OnResetTagSelection(wxCommandEvent&) {
    data.tags_selected.clear();
    refresh_shown();
    redisplay();
}

void Frame::build_tag_gui() {
    // TODO: Repurpose existing elements (change string in-place)
    // without having to clear and re-create entire tags array and tag
    // list each time.
    std::unordered_set<size_t> tags_selected_list_index;
    tag_string_array.Clear();
    tag_list_indices.clear();
    for (size_t tag_index : data.tags_shown) {
        Tag& tag = data.tags.at(tag_index);
        if (data.tags_selected.contains(tag_index)) {
            tags_selected_list_index.insert(tag_string_array.GetCount());
        }
        // Store conversion from index into data.tags vector to index
        // into shown tags list.
        tag_list_indices[tag_string_array.GetCount()] = tag_index;
        tag_string_array.Add(tag.tag);
    }
    // Create new tag list from tags array.
    tag_list->Set(tag_string_array);
    for (size_t selected_tag_list_index : tags_selected_list_index) {
        tag_list->SetSelection(selected_tag_list_index);
    }
}

void Frame::build_entry_gui() {
    // TODO: Repurpose existing buttons, only changing strings
    // in-place, to reduce time spent destroying and re-building
    // buttons.
    entry_list->Clear(true);
    for (size_t entry_index : data.entries_shown) {
        const Entry& entry = data.entries.at(entry_index);
        auto* button = new wxButton(entry_sizer->GetStaticBox(),
                                    entry.index, entry.filepath.string());
        entry_list->Add(button, 1, 0);
    }
    entry_list->Layout();
}

void Frame::OnDirectoryChange(wxCommandEvent&) {
    get_data_and_refresh_gui(dir_picker->GetPath().ToStdString());
}

void Frame::OnTagSelectionChange(wxCommandEvent &event)
{
    //fmt::print("\nindex conversion map:\n");
    //for (const auto& [tag_list_index, tag_data_index] : tag_list_indices) {
    //    fmt::print("  ({},{})\n", tag_list_index, tag_data_index);
    //}

    size_t tag_data_index = tag_list_indices.at(event.GetInt());

    const Tag& tag = data.tags.at(tag_data_index);

    if (event.IsSelection()) {
        // Selected.
        // Add tag to selected tags.
        data.tags_selected.insert(tag.index);
    } else {
        // Deselected.
        // Remove tag from selected tags.
        data.tags_selected.erase(tag.index);
    }

    // Update entries shown based on set intersection of all selected tags' entries.
    // Update tags shown based on set union of all shown entries' tags.
    refresh_shown();

    //fmt::print("\nSelected:\n");
    //for (size_t tag_index : data.tags_selected) {
    //    const Tag& tag = data.tags.at(tag_index);
    //    fmt::print("{} - {}\n", tag.tag, tag.index);
    //}
    //    fmt::print("\nShown:\n");
    //    for (size_t tag_index : tags_shown) {
    //        const Tag& tag = tag_data.at(tag_index);
    //        fmt::print("{} - {}\n", tag.tag, tag.index);
    //    }
    //    fmt::print("\nShown Entries:\n");
    //    for (size_t entry_index : entries_shown) {
    //        const Entry& entry = entry_data.at(entry_index);
    //        fmt::print("{} - {}\n", entry.filepath.string(), entry.index);
    //    }

    redisplay();
}

void Frame::OnEntryClick(wxCommandEvent &event)
{
    const Entry& entry = data.entries.at(event.GetId());

# if defined (_WIN32)
    const auto filepath = entry.filepath;
    std::string fmt_str{""};
    if (is_regular_file(filepath)) {
        fmt_str = "explorer.exe \"{}\" , /select";
    } else {
        fmt_str = "explorer.exe \"{}\"";
    }
# else
    const auto filepath = is_directory(entry.filepath) ? entry.filepath : entry.filepath.parent_path();

#   if defined (__APPLE__)
    static constexpr auto fmt_str = "open '{}'";
#   elif defined (__linux__)
    static constexpr auto fmt_str = "xdg-open '{}'";
#   else
#     error "Platform is not supported; can not implement file-opening feature."
# endif
# endif

    const auto command = fmt::format(fmt_str, filepath.string());
    //fmt::print("Running command: {}\n", command);
    system(command.data());
    //fmt::print("{} - {}\n", entry.filepath.string(), entry.index);
}

void Frame::get_data(std::string dir_path) {
    if (!fs::is_directory(dir_path)) {
        // TODO: Open a message dialog/error dialog to tell the user they must select a directory.
        fmt::print(stderr, "Here be dragons:: dir_path was not a directory!\n");
        return;
    }

    data = get_directory_tagged_entries(dir_path, TaggedEntriesRecursion::No);

    //fmt::print("Tags\n");
    //for (const Tag& it : data.tags) {
    //    fmt::print("{}\n", it.tag);
    //    for (size_t entry_index : it.entries) {
    //        fmt::print("  {}\n", data.entries.at(entry_index).filepath.string());
    //    }
    //}

    //fmt::print("Entries\n");
    //for (const auto& it : data.entries) {
    //    fmt::print("{}\n", it.filepath.string());
    //    for (size_t tag_index : it.tags) {
    //        fmt::print("  {}\n", data.tags.at(tag_index).tag);
    //    }
    //}
}

enum {
    ID_RESET_TAG_SELECTION,
    ID_REFRESH_FROM_DIRECTORY,
    ID_OPEN_DIRECTORY,
};

void Frame::OpenDirectoryDialog(wxCommandEvent&) {
    wxFileDialog dir_dialog{this, _("Search a new directory for tags"), dir_picker->GetPath()};
    if (dir_dialog.ShowModal() == wxID_CANCEL) {
        return;
    }
    std::string dir_path = dir_dialog.GetPath().ToStdString();
    dir_picker->SetPath(dir_path);
    get_data_and_refresh_gui(dir_path);
}

void Frame::create_menubar() {
    wxMenu *file_menu = new wxMenu;
    file_menu->Append(wxID_EXIT);
    file_menu->Append(ID_RESET_TAG_SELECTION, "Reset Selected Tags");
    file_menu->Append(ID_REFRESH_FROM_DIRECTORY, "Refresh From Directory");
    file_menu->Append(ID_OPEN_DIRECTORY, "Open Directory...");

    wxMenuBar *menubar = new wxMenuBar;
    menubar->Append(file_menu, "&File");

    Frame::SetMenuBar(menubar);

    Bind(wxEVT_MENU, &Frame::OnExit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &Frame::OnResetTagSelection, this, ID_RESET_TAG_SELECTION);
    Bind(wxEVT_MENU, &Frame::OnDirectoryChange, this, ID_REFRESH_FROM_DIRECTORY);
    Bind(wxEVT_MENU, &Frame::OpenDirectoryDialog, this, ID_OPEN_DIRECTORY);
}

Frame::Frame(std::string dir_path)
    : wxFrame(nullptr, 0, "C-NOTE")
{
    Frame::SetFont(GetFont().Scale(1.5));

    create_menubar();

    // Create layout
    layout = new wxBoxSizer(wxVERTICAL);
    tags_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Tags");
    entry_sizer = new wxStaticBoxSizer(wxVERTICAL, this, "Entries");
    entry_list = new wxWrapSizer(wxHORIZONTAL);
    entry_list->SetContainingWindow(entry_sizer->GetStaticBox());

    // Create widgets

    dir_picker = new wxDirPickerCtrl(this, wxID_ANY, dir_path, "Directory to Search");

    tag_list = new wxListBox
        (tags_sizer->GetStaticBox(), 0,
         wxDefaultPosition, wxDefaultSize,
         0, NULL, wxLB_MULTIPLE);

    // Get data, build GUI

    get_data_and_refresh_gui(dir_path);

    // Overall layout.
    tags_sizer->Add(tag_list, 1, wxEXPAND);
    entry_sizer->Add(entry_list, 1, wxEXPAND);

    layout->Add(dir_picker, 0, wxEXPAND);
    layout->Add(tags_sizer, 1, wxEXPAND);
    layout->Add(entry_sizer, 0, wxEXPAND);

    layout->Layout();
    Frame::SetSizer(layout);
    Frame::Fit();

    Bind(wxEVT_LISTBOX, &Frame::OnTagSelectionChange, this);
    Bind(wxEVT_BUTTON, &Frame::OnEntryClick, this);
    Bind(wxEVT_DIRPICKER_CHANGED, &Frame::OnDirectoryChange, this);
}

bool App::OnInit() {
    std::string dir_path{"."};
    // If any amount of arguments are passed.
    if (argc > 1) {
        // Use the first argument passed as a directory to start in, if it is one.
        dir_path = std::string{argv[1]};
    }
    // Do NOT delete this. frame->Show() makes it so that the library takes care
    // of delete’ing the frame when appropriate.
    auto* frame = new Frame(dir_path);
    frame->Show(true);
    return true;
}

void Frame::OnExit(wxCommandEvent&) {
    Close(true);
}

wxIMPLEMENT_APP(App);
