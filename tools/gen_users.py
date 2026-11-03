#!/usr/bin/env python3
"""Seeds data/users_db.csv.

Admin keeps the ORIGINAL educational hash on purpose, so the very first web login
demonstrates the automatic migration to PBKDF2-HMAC-SHA256.
Citizen demo accounts are created directly in the modern format.
"""
import hashlib, os, secrets, sys

def legacy_hash(password: str) -> str:
    h = 5381
    for b in password.encode():
        h = ((h << 5) + h + b) & 0xFFFFFFFFFFFFFFFF
    h ^= 0x5BF03635
    return format(h, "016x")

def pbkdf2_hash(password: str, iterations: int = 120000) -> str:
    salt = secrets.token_hex(16)
    dk = hashlib.pbkdf2_hmac("sha256", password.encode(), salt.encode(), iterations, 32).hex()
    return "pbkdf2$sha256$%d$%s$%s" % (iterations, salt, dk)

ACCOUNTS = [
    ("Admin", "admin123", "ADMIN", "legacy"),
    ("citizen.ravi", "ravi1234", "PERSON", "modern"),
    ("citizen.meera", "meera1234", "PERSON", "modern"),
    ("citizen.arjun", "arjun1234", "PERSON", "modern"),
    ("citizen.fatima", "fatima1234", "PERSON", "modern"),
]

def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    data = os.path.join(root, "data")
    os.makedirs(data, exist_ok=True)
    lines = ["username,password_hash,role"]
    for user, pw, role, kind in ACCOUNTS:
        h = legacy_hash(pw) if kind == "legacy" else pbkdf2_hash(pw)
        lines.append("%s,%s,%s" % (user, h, role))
    with open(os.path.join(data, "users_db.csv"), "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print("users seeded:", ", ".join(a[0] for a in ACCOUNTS))

if __name__ == "__main__":
    sys.exit(main())
