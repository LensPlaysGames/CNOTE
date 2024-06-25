/// CLI for CNOTE file tagging library

/// cnote foo
/// -> list all files tagged with `foo`

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <cnote/cnote.h>

void print_help(const char* argv_0) {
    std::printf("USAGE: %s [TAGS...]\n", argv_0);
}

void traverse_directory(
    cnote::Context& ctx,
    const std::string_view dirpath,
    std::vector<std::string_view> filter_tags = {},
    bool should_recurse = false
) {
    for (auto entry : std::filesystem::directory_iterator(dirpath)) {
        if (should_recurse and entry.is_directory())
            traverse_directory(ctx, entry.path().string(), filter_tags, should_recurse);
        if (not entry.is_regular_file()) continue;
        ctx.traverse_file(entry.path().string(), filter_tags);
    }
}

int main(int argc, const char** argv) {
    struct Options {
        bool should_recurse{false};
        std::vector<std::string_view> tags{};
    } options;

    for (int arg_i = 1; arg_i < argc; ++arg_i) {
        auto arg = std::string_view(argv[arg_i]);
        if (arg.substr(0, 2) == "-h" or arg.substr(0, 3) == "--h") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "-r" or arg == "--recurse") {
            options.should_recurse = true;
        } else {
            // Otherwise, it's a query tag.
            options.tags.push_back(arg);
        }
    }

    cnote::Context ctx{};

    // TODO:
    /// MODIFY MODE (+, - subcommands)
    /// Easily modify tagfile entries
    // cnote + foo.txt my-new-tag
    //     Results in .tag entry: "foo.txt #: [...OLD TAGS] my-new-tag"
    // cnote - foo.txt my-new-tag
    //     Removes tag from .tag entry, may remove entry if no tags left.

    /// STANDARD MODE (no subcommand)
    /// Just read-out all the entries

    // Use Entry::MaybeCreate as seen above on all regular files within
    // working directory.
    traverse_directory(ctx, ".", options.tags, options.should_recurse);

    // Also create/update entries based on the .tag dotfile.
    ctx.tagfile(".", options.tags);

    for (auto entry : ctx.entries) {
        std::printf("%s:", entry.filepath.string().data());
        for (auto tag_i : entry.tags) {
            auto tag = ctx.tags[tag_i];
            std::printf(" %s", tag.text.data());
        }
        std::printf("\n");
    }

    return 0;
}
