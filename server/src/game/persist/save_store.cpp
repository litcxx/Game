#include "persist/save_store.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <format>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>
#include <vector>

#include "utils/sha256.hpp"

namespace lit::game {
namespace fs = std::filesystem;

namespace {
// The frame: the magic, the payload's size (8 bytes, little-endian), its SHA-256.
constexpr std::string_view kMagic = "TERRSAVE";
constexpr std::size_t kSizeBytes = 8;
constexpr std::size_t kHeaderBytes = kMagic.size() + kSizeBytes + Sha256{}.size();

std::string frame(std::string_view payload) {
    std::string out(kMagic);
    std::uint64_t size = payload.size();
    for (std::size_t i = 0; i < kSizeBytes; ++i, size >>= 8U) {
        out += static_cast<char>(size & 0xFFU);
    }
    const Sha256 digest = sha256(payload);
    out.append(reinterpret_cast<const char*>(digest.data()), digest.size());
    out += payload;
    return out;
}

// The payload of a framed file, or what is wrong with it.
std::expected<std::string, std::string> unframe(const std::string& bytes) {
    if (bytes.size() < kHeaderBytes || !bytes.starts_with(kMagic)) {
        return std::unexpected("not a world save (no TERRSAVE header)");
    }
    std::uint64_t size = 0;
    for (std::size_t i = kSizeBytes; i-- > 0;) {
        size = (size << 8U) | static_cast<unsigned char>(bytes[kMagic.size() + i]);
    }
    const std::size_t have = bytes.size() - kHeaderBytes;
    if (have != size) {
        return std::unexpected(
            std::format("cut short or overlong: {} payload bytes of {}", have, size));
    }
    std::string payload = bytes.substr(kHeaderBytes);
    const Sha256 digest = sha256(payload);
    if (std::memcmp(digest.data(), bytes.data() + kMagic.size() + kSizeBytes, digest.size()) != 0) {
        return std::unexpected("damaged: its SHA-256 does not match");
    }
    return payload;
}

// The number in world-<n>.save; nullopt for any other name (a .tmp too).
std::optional<std::uint32_t> save_number(const fs::path& file) {
    const std::string name = file.filename().string();
    constexpr std::string_view kPrefix = "world-";
    constexpr std::string_view kSuffix = ".save";
    if (!name.starts_with(kPrefix) || !name.ends_with(kSuffix)) return std::nullopt;
    const std::string digits =
        name.substr(kPrefix.size(), name.size() - kPrefix.size() - kSuffix.size());
    if (digits.empty() ||
        !std::ranges::all_of(digits, [](char c) { return c >= '0' && c <= '9'; })) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(std::stoul(digits));
}

// The saves in `dir`, oldest first.
std::vector<std::pair<std::uint32_t, fs::path>> saves_in(const fs::path& dir,
                                                         std::error_code& error) {
    std::vector<std::pair<std::uint32_t, fs::path>> saves;
    for (const auto& entry : fs::directory_iterator(dir, error)) {
        if (const auto n = save_number(entry.path())) saves.emplace_back(*n, entry.path());
    }
    std::ranges::sort(saves);
    return saves;
}

std::string errno_text(std::string_view what, const fs::path& path) {
    return std::format("{} {}: {}", what, path.string(), std::strerror(errno));
}

// Writes `bytes` to a new file and fsyncs it.
std::expected<void, std::string> write_synced(const fs::path& file, const std::string& bytes) {
    const int fd = ::open(file.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) return std::unexpected(errno_text("cannot create", file));
    std::size_t done = 0;
    while (done < bytes.size()) {
        const ssize_t n = ::write(fd, bytes.data() + done, bytes.size() - done);
        if (n < 0) {
            if (errno == EINTR) continue;
            auto error = errno_text("cannot write", file);
            ::close(fd);
            return std::unexpected(std::move(error));
        }
        done += static_cast<std::size_t>(n);
    }
    if (::fsync(fd) != 0) {
        auto error = errno_text("cannot fsync", file);
        ::close(fd);
        return std::unexpected(std::move(error));
    }
    if (::close(fd) != 0) return std::unexpected(errno_text("cannot close", file));
    return {};
}

// A rename is durable only once its directory is synced.
std::expected<void, std::string> sync_dir(const fs::path& dir) {
    const int fd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) return std::unexpected(errno_text("cannot open", dir));
    const bool synced = ::fsync(fd) == 0;
    auto error = synced ? std::string{} : errno_text("cannot fsync", dir);
    ::close(fd);
    if (!synced) return std::unexpected(std::move(error));
    return {};
}
}  // namespace

SaveStore::SaveStore(fs::path dir, std::uint32_t keep) : dir_{std::move(dir)}, keep_{keep} {}

std::expected<fs::path, std::string> SaveStore::write(std::string_view payload) {
    std::error_code error;
    fs::create_directories(dir_, error);
    if (error)
        return std::unexpected(std::format("cannot make {}: {}", dir_.string(), error.message()));
    auto saves = saves_in(dir_, error);
    if (error)
        return std::unexpected(std::format("cannot list {}: {}", dir_.string(), error.message()));

    const std::uint32_t number = saves.empty() ? 1 : saves.back().first + 1;
    const fs::path file = dir_ / std::format("world-{:08}.save", number);
    fs::path tmp = file;
    tmp += ".tmp";
    if (auto written = write_synced(tmp, frame(payload)); !written) {
        fs::remove(tmp, error);
        return std::unexpected(written.error());
    }
    fs::rename(tmp, file, error);
    if (error) {
        return std::unexpected(std::format("cannot rename {}: {}", tmp.string(), error.message()));
    }
    if (auto synced = sync_dir(dir_); !synced) return std::unexpected(synced.error());

    // Only now, the new one durable: the oldest beyond `keep`, and any .tmp a crash left.
    saves.emplace_back(number, file);
    const std::size_t extra = saves.size() > keep_ ? saves.size() - keep_ : 0;
    for (std::size_t i = 0; i < extra; ++i) fs::remove(saves[i].second, error);
    for (const auto& entry : fs::directory_iterator(dir_, error)) {
        if (entry.path().extension() == ".tmp") fs::remove(entry.path(), error);
    }
    return file;
}

std::expected<std::optional<StoredSave>, std::string> SaveStore::load_latest() const {
    std::error_code error;
    if (!fs::exists(dir_, error)) return std::nullopt;
    const auto saves = saves_in(dir_, error);
    if (error)
        return std::unexpected(std::format("cannot list {}: {}", dir_.string(), error.message()));
    if (saves.empty()) return std::nullopt;

    const fs::path& file = saves.back().second;
    std::ifstream in(file, std::ios::binary);
    if (!in) return std::unexpected(std::format("{}: cannot be read", file.string()));
    const std::string bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    auto payload = unframe(bytes);
    if (!payload) return std::unexpected(std::format("{}: {}", file.string(), payload.error()));
    return StoredSave{file, *std::move(payload)};
}
}  // namespace lit::game
