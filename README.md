# Claims Management System — ProjectSAS

**Author:** Bilal El Yazidi  
**Language:** C (C11)  
**Type:** Console application  

---

## Description

A console-based **Claims Management System** written in C. Users can submit, track, search, and manage customer claims. The system enforces role-based access control, automatic priority assignment, account lockout protection, and session expiry.

All data is persisted to plain-text files so that it survives program restarts.

---

## Features

| Feature | Details |
|---------|---------|
| 🔐 Authentication | Login / Sign-up with account lockout after 3 failed attempts (30 min cooldown) |
| 🔑 Password Policy | ≥ 8 chars, uppercase, lowercase, digit, special char; must not contain username |
| ⏱️ Session Management | Automatic session expiry after 30 minutes of inactivity |
| 📝 Claim Submission | Submit claims with description, reason, and category |
| 🤖 Auto Priority | Priority inferred from keywords in the description (see algorithm below) |
| 🗂️ Role-based Access | Three roles with distinct permissions (Admin / Claim Agent / Client) |
| 🔍 Search | Filter claims by ID, username, category, status, priority, or date |
| 📊 Statistics | Priority/status/category counts with percentages — saved to file |
| 📅 Daily Report | Lists today's new and resolved claims — saved to file |
| 💾 Persistence | All users and claims stored in `.txt` files |

---

## User Roles

| Role | Permissions |
|------|------------|
| **Admin** | All features: manage users, generate stats & reports, manage all claims |
| **Claim Agent** | Search claims, update claim status (In Progress / Resolved / Rejected) |
| **Client** | Submit claims, view own claims, edit/delete own claims within 24 hours |

---

## Getting Started

### Prerequisites

- GCC (MinGW on Windows, or any C11-compatible compiler)

### Build

```bash
gcc -Wall -Wextra -std=c11 -o claims ProjectSAS_Bilal_El_Yazidi.c
```

### Run

```bash
./claims          # Linux / macOS
claims.exe        # Windows
```

### Default Admin Credentials

| Username | Password  |
|----------|-----------|
| `admin`  | `Admin!123` |

> **Note:** The admin account is created automatically on first launch if no admin exists.

> **Important:** If upgrading from a previous version, delete `users.txt` before running.  
> The file format changed (7 fields per user instead of 6) to persist lockout timestamps correctly.

---

## Architecture

### Data Structures

```
UserArray   →  dynamic array of User structs
ClaimArray  →  dynamic array of Claim structs
```

Both arrays start with a capacity of 10 and **double** when full (amortised O(1) insertion).

#### `struct User`
| Field | Type | Description |
|-------|------|-------------|
| `username` | `char[50]` | Unique login name |
| `password` | `char[50]` | Plain-text password (stored as-is) |
| `role` | `enum UserRole` | ADMIN / CLAIM_AGENT / CLIENT |
| `last_login` | `time_t` | Timestamp of last successful login |
| `lockoutTime` | `time_t` | When the lockout started (0 if not locked) |
| `failed_attempts` | `int` | Consecutive wrong passwords since last success |
| `is_locked` | `int` | 1 = account locked, 0 = normal |

#### `struct Claim`
| Field | Type | Description |
|-------|------|-------------|
| `id` | `int` | Auto-incremented unique identifier |
| `username` | `char[50]` | Owner of the claim |
| `description` | `char[500]` | Full description text |
| `category` | `enum ClaimCategory` | PAYMENT / CUSTOMER_SERVICES / TECHNICAL |
| `reason` | `char[100]` | Short reason for the claim |
| `status` | `enum ClaimStatus` | PENDING / IN_PROGRESS / RESOLVED / REJECTED |
| `priority` | `enum ClaimPriority` | LOW / MEDIUM / HIGH (auto-inferred) |
| `submission_date` | `time_t` | When the claim was created |
| `last_status_change` | `time_t` | When the status last changed |
| `resolution_note` | `char[500]` | Note added by agent/admin when changing status |

---

## Algorithms

### Priority Inference

When a claim is submitted (or its description is edited), priority is assigned automatically by scanning the description for trigger keywords (case-insensitive):

```
HIGH   ← "urgent"  | "emergency" | "critical" | "severe"
MEDIUM ← "important" | "significant" | "moderate"
LOW    ← (everything else)
```

Implemented in `inferPriorityFromDescription()` — called from both `submitClaim()` and `manageClaims()`.

### Claim Sorting

Claims are sorted with `qsort` using the `compareClaims` comparator:

1. **Primary key:** Priority descending (`HIGH` → `MEDIUM` → `LOW`)
2. **Tiebreak:** Submission date descending (newest first)

### Account Lockout

1. Each wrong password increments `failed_attempts`.
2. When `failed_attempts >= 3`, `is_locked = 1` and `lockoutTime = now`.
3. On the next login attempt, if `now - lockoutTime >= 1800s`, the lock resets automatically.
4. Lockout state survives restarts because it is persisted in `users.txt`.

### Session Expiry

After login, `last_login` is set to `time(NULL)`. Before every authenticated menu action, `isSessionValid()` checks `now - last_login <= 1800s`. If expired, the user is forced to log in again.

---

## File Structure

```
ProjectSAS_Bilal_El_Yazidi/
│
├── ProjectSAS_Bilal_El_Yazidi.c   # Main source file (all logic)
│
├── users.txt                      # Persisted user accounts (auto-created)
├── claims.txt                     # Persisted claims (auto-created)
├── statistics.txt                 # Output of "Generate Statistics" (auto-created)
├── daily_report.txt               # Output of "Generate Daily Report" (auto-created)
│
└── README.md                      # This file
```

### `users.txt` format

```
<count>
username,password,role,last_login,lockoutTime,failed_attempts,is_locked
...
```

### `claims.txt` format

```
<count>
id,username,description,category,reason,status,priority,submission_date,last_status_change,resolution_note
...
```

---

## Known Limitations

| Limitation | Details |
|-----------|---------|
| Plain-text passwords | Passwords are stored without hashing — not suitable for production |
| No commas in descriptions | Commas in description/reason fields break the CSV-based file format |
| Single-file architecture | All code lives in one `.c` file — would benefit from header/source split |
| No concurrent access | No file locking; running two instances simultaneously will corrupt data |

---

## Future Improvements

- [ ] Hash passwords (e.g., SHA-256 or bcrypt)
- [ ] Use a proper CSV/JSON serialiser that handles commas in field values
- [ ] Split into multiple files: `auth.c`, `claims.c`, `reports.c`, `utils.c`
- [ ] Add claim assignment — agents claim specific tickets
- [ ] Add comment threads on a claim
- [ ] Export statistics to CSV for spreadsheet analysis
