#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <string>

#include "persist/save_store.hpp"

namespace fs = std::filesystem;

namespace {

// A directory of its own for each test, gone after it.
struct TempDir {
    fs::path path =
        fs::temp_directory_path() /
        ("lit-save-" + std::string{testing::UnitTest::GetInstance()->current_test_info()->name()} +
         "-" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()));
    TempDir() { fs::remove_all(path); }
    ~TempDir() { fs::remove_all(path); }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
};

std::set<std::string> files_in(const fs::path& dir) {
    std::set<std::string> names;
    for (const auto& entry : fs::directory_iterator(dir)) names.insert(entry.path().filename());
    return names;
}

std::string read(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void overwrite(const fs::path& file, const std::string& bytes) {
    std::ofstream(file, std::ios::binary | std::ios::trunc) << bytes;
}

// The newest save's payload, or what went wrong.
std::string newest(const lit::game::SaveStore& store) {
    const auto loaded = store.load_latest();
    if (!loaded) return "error: " + loaded.error();
    if (!*loaded) return "none";
    return (*loaded)->payload;
}

}  // namespace

TEST(SaveStore, LoadsTheNewest) {
    TempDir dir;
    lit::game::SaveStore store(dir.path, 5);

    ASSERT_TRUE(store.write("first"));
    const auto second = store.write("second");

    ASSERT_TRUE(second);
    EXPECT_EQ(second->filename(), "world-00000002.save");
    EXPECT_EQ(newest(store), "second");
}

TEST(SaveStore, NoDirectoryOrNoFileIsNoSave) {
    TempDir dir;
    const lit::game::SaveStore store(dir.path, 5);
    EXPECT_EQ(newest(store), "none");

    fs::create_directories(dir.path);
    EXPECT_EQ(newest(store), "none");
}

TEST(SaveStore, KeepsTheNewestOnes) {
    TempDir dir;
    lit::game::SaveStore store(dir.path, 3);

    for (int i = 1; i <= 5; ++i) ASSERT_TRUE(store.write("save " + std::to_string(i)));

    EXPECT_EQ(files_in(dir.path),
              (std::set<std::string>{"world-00000003.save", "world-00000004.save",
                                     "world-00000005.save"}));
    EXPECT_EQ(newest(store), "save 5");
}

TEST(SaveStore, TheNumbersGoOnAfterARestart) {
    TempDir dir;
    ASSERT_TRUE(lit::game::SaveStore(dir.path, 5).write("before"));

    lit::game::SaveStore again(dir.path, 5);
    const auto written = again.write("after");

    ASSERT_TRUE(written);
    EXPECT_EQ(written->filename(), "world-00000002.save");
}

// --- A file that isn't whole is refused: the operator decides --------------------

TEST(SaveStore, ACutFileIsRefused) {
    TempDir dir;
    lit::game::SaveStore store(dir.path, 5);
    ASSERT_TRUE(store.write("older"));
    const auto file = store.write("a world of many cells");
    ASSERT_TRUE(file);
    const std::string bytes = read(*file);
    overwrite(*file, bytes.substr(0, bytes.size() - 3));

    const std::string got = newest(store);
    EXPECT_NE(got.find("error: "), std::string::npos);
    EXPECT_NE(got.find("world-00000002.save"), std::string::npos) << got;  // not the older one
}

TEST(SaveStore, ADamagedFileIsRefused) {
    TempDir dir;
    lit::game::SaveStore store(dir.path, 5);
    const auto file = store.write("a world of many cells");
    ASSERT_TRUE(file);
    std::string bytes = read(*file);
    bytes.back() ^= 0x01;  // one bit of the payload
    overwrite(*file, bytes);

    EXPECT_NE(newest(store).find("error: "), std::string::npos);
}

TEST(SaveStore, SomethingElseUnderASavesNameIsRefused) {
    TempDir dir;
    fs::create_directories(dir.path);
    overwrite(dir.path / "world-00000001.save", "not a save at all");

    EXPECT_NE(newest(lit::game::SaveStore(dir.path, 5)).find("error: "), std::string::npos);
}

// A write cut short by a crash leaves a .tmp file: never loaded, cleared by the next write.
TEST(SaveStore, AnUnfinishedWriteIsIgnored) {
    TempDir dir;
    lit::game::SaveStore store(dir.path, 5);
    ASSERT_TRUE(store.write("whole"));
    overwrite(dir.path / "world-00000002.save.tmp", "half");

    EXPECT_EQ(newest(store), "whole");
    ASSERT_TRUE(store.write("next"));
    EXPECT_EQ(files_in(dir.path),
              (std::set<std::string>{"world-00000001.save", "world-00000002.save"}));
}

TEST(SaveStore, AWriteWhereThereIsNoRoomFails) {
    TempDir dir;
    fs::create_directories(dir.path.parent_path());
    overwrite(dir.path, "a file where the directory should be");
    lit::game::SaveStore store(dir.path, 5);

    const auto written = store.write("anything");

    ASSERT_FALSE(written);
    EXPECT_FALSE(written.error().empty());
    fs::remove(dir.path);
}
