#include <gtest/gtest.h>

#include "sha256.hpp"

TEST(SHA256Tests, GenerateSalt) {
    auto salt = lit::crypto::generate_salt();

    EXPECT_NE(salt, std::nullopt);
    EXPECT_EQ(salt.value().size(), static_cast<std::size_t>(lit::crypto::Crypto::kSaltLength));
}

TEST(SHA256Tests, Hash) {
    auto salt = lit::crypto::generate_salt();

    auto hash = lit::crypto::hash("user123", salt.value());

    EXPECT_NE(hash, std::nullopt);
    EXPECT_EQ(hash.value().size(), static_cast<std::size_t>(lit::crypto::Crypto::kHashLength));
}

TEST(SHA256Tests, VerifyHash) {
    auto salt = lit::crypto::generate_salt();

    std::string password1("user123");
    std::string password2("user124");

    auto hash = lit::crypto::hash(password1, salt.value());

    EXPECT_EQ(lit::crypto::verify_hash(password1, hash.value(), salt.value()), true);
    EXPECT_NE(lit::crypto::verify_hash(password2, hash.value(), salt.value()), true);
}
