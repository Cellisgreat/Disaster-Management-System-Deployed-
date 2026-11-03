/* ============================================================================
 *  crypto.h  --  Password hashing and secure token generation
 * ----------------------------------------------------------------------------
 *  The original project stored passwords with a small hand-written
 *  "educational" hash (unsalted djb2). That is not acceptable for a web
 *  application, so the web release uses:
 *
 *      PBKDF2-HMAC-SHA256, 120 000 iterations, 16 byte random salt,
 *      stored as:  pbkdf2$sha256$<iterations>$<salt-hex>$<hash-hex>
 *
 *  Nothing external is required: SHA-256 / HMAC / PBKDF2 are implemented here
 *  (public-domain style implementation) and CSPRNG tokens come from
 *  std::random_device seeded mt19937_64, which is sufficient for this
 *  educational deployment.
 *
 *  Legacy accounts are migrated automatically: the first successful login of
 *  an account whose stored hash is in the old format re-hashes the password
 *  with PBKDF2 and rewrites users_db.csv.
 * ==========================================================================*/
#ifndef DMS_CRYPTO_H
#define DMS_CRYPTO_H

#include <string>

namespace dms {

/* --- SHA-256 / HMAC ------------------------------------------------------ */
std::string sha256Hex(const std::string &input);
std::string hmacSha256Hex(const std::string &key, const std::string &message);

/* --- password hashing ---------------------------------------------------- */
/** Hash a password with PBKDF2-HMAC-SHA256 and return the storable string. */
std::string hashPassword(const std::string &password, int iterations = 120000);

/** Verify a password against a stored hash (PBKDF2 or legacy educational). */
bool verifyPassword(const std::string &password, const std::string &storedHash);

/** True when the stored hash uses the legacy (unsafe) educational format. */
bool isLegacyHash(const std::string &storedHash);

/** The historical educational hash, kept only to migrate old accounts. */
std::string legacyEducationalHash(const std::string &password);

/* --- misc ---------------------------------------------------------------- */
/** Cryptographically seeded random hex string (used for session tokens). */
std::string randomHex(std::size_t bytes);

/** Constant-time string comparison (avoids timing side channels). */
bool constantTimeEquals(const std::string &a, const std::string &b);

/** Hex helpers (also used by the session store / API). */
std::string toHex(const unsigned char *data, std::size_t len);
std::string pbkdf2Sha256Hex(const std::string &password, const std::string &salt,
                            int iterations, std::size_t derivedBytes);

}  /* namespace dms */

#endif  /* DMS_CRYPTO_H */
