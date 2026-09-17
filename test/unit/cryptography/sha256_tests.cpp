#include <gtest/gtest.h>

#include "sha256.hpp"

TEST(SHA256Tests, GenerateSalt) {
    auto salt = ep::crypto::generate_salt();

    EXPECT_NE(salt, std::nullopt);
    EXPECT_EQ(salt.value().size(), static_cast<std::size_t>(ep::crypto::Crypto::kSaltLength));
}

TEST(SHA256Tests, Hash) {
    auto salt = ep::crypto::generate_salt();

    auto hash = ep::crypto::hash("user123", salt.value());

    EXPECT_NE(hash, std::nullopt);
    EXPECT_EQ(hash.value().size(), static_cast<std::size_t>(ep::crypto::Crypto::kHashLength));
}

TEST(SHA256Tests, VerifyHash) {
    auto salt = ep::crypto::generate_salt();

    std::string password1("user123");
    std::string password2("user124");

    auto hash = ep::crypto::hash(password1, salt.value());

    EXPECT_EQ(ep::crypto::verify_hash(password1, hash.value(), salt.value()), true);
    EXPECT_NE(ep::crypto::verify_hash(password2, hash.value(), salt.value()), true);
}
