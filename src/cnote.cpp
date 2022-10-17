#include "cnote.h"

Tag::Tag(std::string_view tag_string): tag(tag_string) { }

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

