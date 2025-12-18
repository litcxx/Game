#include <gtest/gtest.h>

#include "sha256.hpp"

TEST(SHA256Tests, GenerateSalt)
{
  auto salt = ep::crypto::GenerateSalt();

  EXPECT_NE(salt, std::nullopt);
  EXPECT_EQ(salt.value().size(), static_cast<std::size_t>(ep::crypto::Crypto::kSaltLength));
}

TEST(SHA256Tests, Hash)
{
  auto salt = ep::crypto::GenerateSalt();

  auto hash = ep::crypto::Hash("user123", salt.value());

  EXPECT_NE(hash, std::nullopt);
  EXPECT_EQ(hash.value().size(), static_cast<std::size_t>(ep::crypto::Crypto::kHashLength));
}

TEST(SHA256Tests, VerifyHash)
{
  auto salt = ep::crypto::GenerateSalt();

  std::string password1("user123");
  std::string password2("user124");

  auto hash = ep::crypto::Hash(password1, salt.value());

  EXPECT_EQ(ep::crypto::VerifyHash(password1, hash.value(), salt.value()), true);
  EXPECT_NE(ep::crypto::VerifyHash(password2, hash.value(), salt.value()), true);
}
