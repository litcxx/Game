#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace lit::game {
// A save as read back: which file, and its payload (a serialized WorldSave).
struct StoredSave {
    std::filesystem::path path;
    std::string payload;
};

// The world save's files in one directory (GAME-019): world-00000001.save,
// -00000002, … — the highest number is the newest. Each file frames its payload
// (a header, the payload's size and SHA-256), so a file cut short or damaged is
// told from a whole one. Not thread-safe: one writer (SaveWriter), or a load
// before it starts.
class SaveStore {
  public:
    // `dir` is made by the first write; the newest `keep` (>= 1) files stay.
    SaveStore(std::filesystem::path dir, std::uint32_t keep);

    // Writes `payload` as the newest save, numbered one past the newest there:
    // to a .tmp file, fsync, renamed into place, the directory fsync'ed — a crash
    // leaves the old files whole. Then removes all but the newest `keep` and any
    // leftover .tmp. Returns the file, or why it could not be written.
    std::expected<std::filesystem::path, std::string> write(std::string_view payload);

    // The newest save; nullopt when there is none (no directory, or no save in
    // it). A newest file that is cut short, damaged or not a save is an error:
    // older ones are not tried — what to load is the operator's call.
    std::expected<std::optional<StoredSave>, std::string> load_latest() const;

    const std::filesystem::path& dir() const { return dir_; }

  private:
    std::filesystem::path dir_;
    std::uint32_t keep_;
};
}  // namespace lit::game
