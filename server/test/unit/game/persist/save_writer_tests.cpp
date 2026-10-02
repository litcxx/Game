#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "persist/save_store.hpp"
#include "persist/save_writer.hpp"
#include "save.pb.h"

namespace fs = std::filesystem;

namespace {

struct TempDir {
    fs::path path = fs::temp_directory_path() /
                    ("lit-writer-" +
                     std::string{testing::UnitTest::GetInstance()->current_test_info()->name()});
    TempDir() { fs::remove_all(path); }
    ~TempDir() { fs::remove_all(path); }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
};

lit::save::WorldSave season(std::uint32_t id) {
    lit::save::WorldSave save;
    save.set_version(1);
    save.set_season_id(id);
    return save;
}

// The season of the newest save in `dir` (0: none or unreadable).
std::uint32_t saved_season(const fs::path& dir) {
    const auto loaded = lit::game::SaveStore(dir, 5).load_latest();
    if (!loaded || !*loaded) return 0;
    lit::save::WorldSave save;
    return save.ParseFromString((*loaded)->payload) ? save.season_id() : 0;
}

// Waits up to a second for the writer's stats to show `n` writes or failures.
lit::game::SaveStats wait_for(const lit::game::SaveWriter& writer, std::uint32_t n) {
    for (int i = 0; i < 100; ++i) {
        const auto stats = writer.stats();
        if (stats.written + stats.failed >= n) return stats;
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    return writer.stats();
}

}  // namespace

TEST(SaveWriter, WritesWhatIsSubmittedInTheBackground) {
    TempDir dir;
    lit::game::SaveWriter writer(lit::game::SaveStore(dir.path, 5));

    writer.submit(season(7));

    const auto stats = wait_for(writer, 1);
    EXPECT_EQ(stats.written, 1u);
    EXPECT_EQ(stats.failed, 0u);
    EXPECT_GT(stats.last_bytes, 0u);
    EXPECT_EQ(saved_season(dir.path), 7u);
}

// The save on SIGTERM: submitted just before the writer goes, it is still written.
TEST(SaveWriter, WritesWhatIsPendingBeforeItStops) {
    TempDir dir;
    {
        lit::game::SaveWriter writer(lit::game::SaveStore(dir.path, 5));
        writer.submit(season(8));
    }
    EXPECT_EQ(saved_season(dir.path), 8u);
}

TEST(SaveWriter, AFailedWriteIsCountedAndTheGameGoesOn) {
    TempDir dir;
    fs::create_directories(dir.path.parent_path());
    std::ofstream(dir.path) << "a file where the directory should be";
    {
        lit::game::SaveWriter writer(lit::game::SaveStore(dir.path, 5));
        writer.submit(season(9));

        const auto stats = wait_for(writer, 1);
        EXPECT_EQ(stats.failed, 1u);
        EXPECT_EQ(stats.written, 0u);
    }
    fs::remove(dir.path);
}
