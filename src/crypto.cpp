#include "crypto.h"

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>
#include <vector>

#include "csv_utils.h"

namespace dms {

/* ==========================================================================
 *  SHA-256 (FIPS 180-4) -- compact reference implementation
 * ========================================================================*/
namespace {

inline std::uint32_t rotr(std::uint32_t x, std::uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

const std::uint32_t K[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

void sha256Raw(const unsigned char *data, std::size_t len, unsigned char out[32]) {
    std::uint32_t h[8] = {0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
                          0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};

    std::vector<unsigned char> msg(data, data + len);
    const std::uint64_t bitLen = static_cast<std::uint64_t>(len) * 8ULL;
    msg.push_back(0x80);
    while (msg.size() % 64 != 56) msg.push_back(0x00);
    for (int i = 7; i >= 0; --i)
        msg.push_back(static_cast<unsigned char>((bitLen >> (i * 8)) & 0xFF));

    for (std::size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            const std::size_t p = chunk + static_cast<std::size_t>(i) * 4;
            w[i] = (static_cast<std::uint32_t>(msg[p]) << 24) |
                   (static_cast<std::uint32_t>(msg[p + 1]) << 16) |
                   (static_cast<std::uint32_t>(msg[p + 2]) << 8) |
                    static_cast<std::uint32_t>(msg[p + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i-15],7) ^ rotr(w[i-15],18) ^ (w[i-15] >> 3);
            const std::uint32_t s1 = rotr(w[i-2],17) ^ rotr(w[i-2],19)  ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }

        std::uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t S1 = rotr(e,6) ^ rotr(e,11) ^ rotr(e,25);
            const std::uint32_t ch = (e & f) ^ ((~e) & g);
            const std::uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            const std::uint32_t S0 = rotr(a,2) ^ rotr(a,13) ^ rotr(a,22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }

    for (int i = 0; i < 8; ++i) {
        out[i*4]     = static_cast<unsigned char>((h[i] >> 24) & 0xFF);
        out[i*4 + 1] = static_cast<unsigned char>((h[i] >> 16) & 0xFF);
        out[i*4 + 2] = static_cast<unsigned char>((h[i] >> 8) & 0xFF);
        out[i*4 + 3] = static_cast<unsigned char>(h[i] & 0xFF);
    }
}

void hmacSha256Raw(const std::string &key, const unsigned char *msg, std::size_t msgLen,
                   unsigned char out[32]) {
    unsigned char k[64];
    std::memset(k, 0, sizeof(k));
    if (key.size() > 64) {
        unsigned char keyHash[32];
        sha256Raw(reinterpret_cast<const unsigned char *>(key.data()), key.size(), keyHash);
        std::memcpy(k, keyHash, 32);
    } else if (!key.empty()) {
        std::memcpy(k, key.data(), key.size());
    }

    unsigned char ipad[64], opad[64];
    for (int i = 0; i < 64; ++i) {
        ipad[i] = static_cast<unsigned char>(k[i] ^ 0x36);
        opad[i] = static_cast<unsigned char>(k[i] ^ 0x5c);
    }

    std::vector<unsigned char> inner;
    inner.reserve(64 + msgLen);
    inner.insert(inner.end(), ipad, ipad + 64);
    if (msgLen) inner.insert(inner.end(), msg, msg + msgLen);
    unsigned char innerHash[32];
    sha256Raw(&inner[0], inner.size(), innerHash);

    std::vector<unsigned char> outer;
    outer.reserve(96);
    outer.insert(outer.end(), opad, opad + 64);
    outer.insert(outer.end(), innerHash, innerHash + 32);
    sha256Raw(&outer[0], outer.size(), out);
}

}  /* anonymous namespace */

std::string toHex(const unsigned char *data, std::size_t len) {
    static const char *digits = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (std::size_t i = 0; i < len; ++i) {
        out += digits[(data[i] >> 4) & 0x0F];
        out += digits[data[i] & 0x0F];
    }
    return out;
}

std::string sha256Hex(const std::string &input) {
    unsigned char digest[32];
    sha256Raw(reinterpret_cast<const unsigned char *>(input.data()), input.size(), digest);
    return toHex(digest, 32);
}

std::string hmacSha256Hex(const std::string &key, const std::string &message) {
    unsigned char digest[32];
    hmacSha256Raw(key, reinterpret_cast<const unsigned char *>(message.data()),
                  message.size(), digest);
    return toHex(digest, 32);
}

std::string pbkdf2Sha256Hex(const std::string &password, const std::string &salt,
                            int iterations, std::size_t derivedBytes) {
    const std::size_t blocks = (derivedBytes + 31) / 32;
    std::string out;
    for (std::size_t b = 1; b <= blocks; ++b) {
        unsigned char be[4] = { static_cast<unsigned char>((b >> 24) & 0xFF),
                                static_cast<unsigned char>((b >> 16) & 0xFF),
                                static_cast<unsigned char>((b >> 8) & 0xFF),
                                static_cast<unsigned char>(b & 0xFF) };
        std::vector<unsigned char> first(salt.begin(), salt.end());
        first.insert(first.end(), be, be + 4);

        unsigned char u[32];
        hmacSha256Raw(password, &first[0], first.size(), u);
        unsigned char t[32];
        std::memcpy(t, u, 32);

        for (int i = 1; i < iterations; ++i) {
            hmacSha256Raw(password, u, 32, u);
            for (int j = 0; j < 32; ++j) t[j] ^= u[j];
        }
        out += toHex(t, 32);
    }
    return out.substr(0, derivedBytes * 2);
}

std::string hashPassword(const std::string &password, int iterations) {
    const std::string salt = randomHex(16);
    const std::string derived = pbkdf2Sha256Hex(password, salt, iterations, 32);
    std::ostringstream oss;
    oss << "pbkdf2$sha256$" << iterations << '$' << salt << '$' << derived;
    return oss.str();
}

std::string legacyEducationalHash(const std::string &password) {
    /* The historical educational hash: unsalted djb2 with a magic final mix. */
    unsigned long long h = 5381ULL;
    for (std::size_t i = 0; i < password.size(); ++i)
        h = ((h << 5) + h) + static_cast<unsigned char>(password[i]);
    h ^= 0x5bf03635ULL;
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << h;
    return oss.str();
}

bool isLegacyHash(const std::string &storedHash) {
    return storedHash.rfind("pbkdf2$", 0) != 0;
}

bool verifyPassword(const std::string &password, const std::string &storedHash) {
    if (storedHash.rfind("pbkdf2$sha256$", 0) == 0) {
        /* pbkdf2$sha256$<iterations>$<salt>$<hash> */
        std::vector<std::string> parts;
        std::string current;
        for (std::size_t i = 0; i < storedHash.size(); ++i) {
            if (storedHash[i] == '$') { parts.push_back(current); current.clear(); }
            else current += storedHash[i];
        }
        parts.push_back(current);
        if (parts.size() != 5) return false;
        const int iterations = toInt(parts[2], 0);
        if (iterations <= 0) return false;
        const std::string derived = pbkdf2Sha256Hex(password, parts[3], iterations, 32);
        return constantTimeEquals(derived, parts[4]);
    }
    /* legacy */
    return constantTimeEquals(legacyEducationalHash(password), storedHash);
}

std::string randomHex(std::size_t bytes) {
    std::random_device rd;
    std::vector<unsigned char> buf(bytes);
    for (std::size_t i = 0; i < bytes; ++i)
        buf[i] = static_cast<unsigned char>(rd() & 0xFF);
    return toHex(buf.empty() ? 0 : &buf[0], bytes);
}

bool constantTimeEquals(const std::string &a, const std::string &b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        diff = static_cast<unsigned char>(diff | (a[i] ^ b[i]));
    return diff == 0;
}

}  /* namespace dms */
