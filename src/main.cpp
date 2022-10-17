#if defined (_WIN32) && !defined (__WXMSW__)
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

#include "cnote.h"

namespace ranges = std::ranges;
namespace fs = std::filesystem;
using namespace std::string_literals;
using namespace std::string_view_literals;

struct App : public wxApp {
    virtual bool OnInit() override;
};

struct Frame : public wxFrame {
    Frame();
private:
    void OnEntryClick(wxCommandEvent &event);
    void OnTagSelectionChange(wxCommandEvent &event);
    void OnExit(wxCommandEvent& event);

    TaggedEntries data;
    void get_data();

    void build_tag_gui();
    wxListBox *tag_list;
    wxArrayString tag_string_array;

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
    tag_string_array.Clear();
    tag_list_indices.clear();
    for (size_t tag_index : data.tags_shown) {
        Tag& tag = data.tags.at(tag_index);
        if (data.tags_selected.contains(tag_index)) {
            fmt::print("{} is shown and selected. {}\n", tag_index, tag_string_array.GetCount());
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
        data.entries_shown.clear();
        for (const Entry& entry : data.entries) {
            // If entry is contained in *all* selected tags, make entry shown.
            bool has_all_tags = ranges::all_of(data.tags_selected,
                [&](size_t tag_index){ return data.tags.at(tag_index).entries.contains(entry.index); });
            if (has_all_tags) { data.entries_shown.insert(entry.index); }
        }

        // Update tags shown based on set intersection of all shown entries' tags.
        data.tags_shown.clear();
        for (const Tag& tag : data.tags) {
            auto used = ranges::any_of(data.entries_shown,
                [&](size_t entry_index){ return data.entries.at(entry_index).tags.contains(tag.index); });
            if (used) { data.tags_shown.insert(tag.index); }
        }

        fmt::print("\nSelected:\n");
        for (size_t tag_index : data.tags_selected) {
            const Tag& tag = data.tags.at(tag_index);
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
    fmt::print("{}\n", command);
    system(command.data());

    fmt::print("{} - {}\n", entry.filepath.string(), entry.index);
}

void Frame::get_data() {
    data = get_directory_tagged_entries(".", TaggedEntriesRecursion::No);

    fmt::print("Tags\n");
    for (const Tag& it : data.tags) {
        fmt::print("{}\n", it.tag);
        for (size_t entry_index : it.entries) {
            fmt::print("  {}\n", data.entries.at(entry_index).filepath.string());
        }
    }

    fmt::print("Entries\n");
    for (const auto& it : data.entries) {
        fmt::print("{}\n", it.filepath.string());
        for (size_t tag_index : it.tags) {
            fmt::print("  {}\n", data.tags.at(tag_index).tag);
        }
    }
}

Frame::Frame()
    : wxFrame(nullptr, 0, "C-NOTE")
{
    Frame::SetFont(GetFont().Scale(1.5));

    get_data();

    // Create layout
    layout = new wxBoxSizer(wxVERTICAL);
    entry_list = new wxBoxSizer(wxVERTICAL);

    // Create widgets, fill in data.

    // Populate tag array.
    for (const Tag& it : data.tags) {
        tag_string_array.Add(it.tag.data());
    }

    // Create new tag_list from tag array.
    tag_list = new wxListBox
        (this, 0,
         wxDefaultPosition, wxDefaultSize,
         tag_string_array.Count(), tag_string_array.begin(),
         wxLB_MULTIPLE);

    for (size_t i = 0; i < tag_list->GetCount(); ++i) {
        tag_list_indices[i] = i;
    }

    // Populate entry_list.
    for (const Entry& entry : data.entries) {
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
