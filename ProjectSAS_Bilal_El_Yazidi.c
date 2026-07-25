/**
 * ProjectSAS_Bilal_El_Yazidi.c
 *
 * Claims Management System
 * A console-based application for submitting, tracking, and managing
 * customer claims. Supports three user roles: Admin, Claim Agent, Client.
 *
 * Author : Bilal El Yazidi
 * Build  : gcc -Wall -Wextra -std=c11 -o claims ProjectSAS_Bilal_El_Yazidi.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* ============================================================
 * PORTABLE HELPERS
 * ============================================================ */

/**
 * my_strdup — portable replacement for POSIX strdup().
 * Allocates a copy of src on the heap; caller must free().
 */
static char *my_strdup(const char *src) {
    size_t len = strlen(src) + 1;
    char  *dup = malloc(len);
    if (dup) memcpy(dup, src, len);
    return dup;
}

/**
 * my_strcasecmp — portable, locale-independent case-insensitive string compare.
 * Returns 0 if strings are equal ignoring case, <0 or >0 otherwise.
 */
static int my_strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        int diff = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (diff != 0) return diff;
        a++; b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

/* ============================================================
 * CONSTANTS
 * ============================================================ */

#define MAX_USERNAME_LENGTH    50
#define MAX_PASSWORD_LENGTH    50
#define MAX_DESCRIPTION_LENGTH 500
#define MAX_REASON_LENGTH      100
#define DATE_STRING_LENGTH     20   /* "YYYY-MM-DD HH:MM:SS\0" */

#define USERS_FILE        "users.txt"
#define CLAIMS_FILE       "claims.txt"
#define STATISTICS_FILE   "statistics.txt"
#define DAILY_REPORT_FILE "daily_report.txt"

#define LOCKOUT_SECONDS    1800   /* 30 minutes */
#define SESSION_SECONDS    1800   /* 30 minutes */
#define MAX_LOGIN_ATTEMPTS 3

/* ============================================================
 * ENUMERATIONS
 *
 * Enum values start at 0 so they can be used directly as array
 * indices — no arithmetic adjustments needed.
 * ============================================================ */

enum UserRole {
    ADMIN       = 0,
    CLAIM_AGENT = 1,
    CLIENT      = 2
};

enum ClaimStatus {
    PENDING     = 0,
    IN_PROGRESS = 1,
    RESOLVED    = 2,
    REJECTED    = 3
};

enum ClaimPriority {
    LOW    = 0,
    MEDIUM = 1,
    HIGH   = 2
};

enum ClaimCategory {
    PAYMENT           = 0,
    CUSTOMER_SERVICES = 1,
    TECHNICAL         = 2
};

/* ============================================================
 * DATA STRUCTURES
 * ============================================================ */

struct User {
    char           username[MAX_USERNAME_LENGTH];
    char           password[MAX_PASSWORD_LENGTH];
    enum UserRole  role;
    time_t         last_login;
    time_t         lockoutTime;      /* when the lockout started */
    int            failed_attempts;
    int            is_locked;
};

struct Claim {
    int                  id;
    char                 username[MAX_USERNAME_LENGTH];
    char                 description[MAX_DESCRIPTION_LENGTH];
    enum ClaimCategory   category;
    char                 reason[MAX_REASON_LENGTH];
    enum ClaimStatus     status;
    enum ClaimPriority   priority;
    time_t               submission_date;
    time_t               last_status_change;
    char                 resolution_note[MAX_DESCRIPTION_LENGTH];
};

/* Dynamic arrays (grow by doubling) */
struct UserArray {
    struct User *users;
    int          count;
    int          capacity;
};

struct ClaimArray {
    struct Claim *claims;
    int           count;
    int           capacity;
};

/* ============================================================
 * FUNCTION PROTOTYPES
 * ============================================================ */

/* --- Array management --- */
void initUserArray(struct UserArray *ua);
void initClaimArray(struct ClaimArray *ca);
void addUser(struct UserArray *ua, struct User user);
void addClaim(struct ClaimArray *ca, struct Claim claim);

/* --- File persistence ---
 * File format — users (7 comma-separated fields per line):
 *   username,password,role,last_login,lockoutTime,failed_attempts,is_locked
 * File format — claims (10 comma-separated fields per line):
 *   id,username,description,category,reason,status,priority,
 *   submission_date,last_status_change,resolution_note
 */
void saveUsersToFile(struct UserArray *ua, const char *filename);
void loadUsersFromFile(struct UserArray *ua, const char *filename);
void saveClaimsToFile(struct ClaimArray *ca, const char *filename);
void loadClaimsFromFile(struct ClaimArray *ca, const char *filename);

/* --- Authentication --- */
int  authenticateUser(struct UserArray *ua, const char *username, const char *password);
int  isSessionValid(time_t lastLogin);
int  registerUser(struct UserArray *ua);
int  isPasswordValid(const char *password, const char *username);

/* --- Date helpers --- */
void   formatDate(time_t timestamp, char *dateStr);
time_t parseDate(const char *dateStr);

/* --- Priority inference --- */
enum ClaimPriority inferPriorityFromDescription(const char *description);

/* --- Claim operations --- */
void submitClaim(struct ClaimArray *ca, const char *username);
void displayUserClaims(struct ClaimArray *ca, const char *username, enum UserRole role);
void searchClaims(struct ClaimArray *ca, enum UserRole role);
void manageClaims(struct ClaimArray *ca, const char *username, enum UserRole role, int claim_id);
int  compareClaims(const void *a, const void *b);

/* --- Admin operations --- */
void manageUsers(struct UserArray *ua);
void generateStatistics(struct ClaimArray *ca, const char *filename);
void generateDailyReport(struct ClaimArray *ca, const char *filename);

/* --- Enum → string helpers --- */
const char *getPriorityString(enum ClaimPriority priority);
const char *getCategoryString(enum ClaimCategory category);
const char *getStatusString(enum ClaimStatus status);

/* --- Display utilities --- */
int  isWithin24Hours(time_t submissionTime);
void printLine(int num);
void printSlashes(int num);
void printAsterics(int num);
void padding(int num);

/* ============================================================
 * MAIN
 * ============================================================ */

int main(void) {
    struct UserArray  ua;
    struct ClaimArray ca;

    initUserArray(&ua);
    initClaimArray(&ca);

    loadUsersFromFile(&ua, USERS_FILE);
    loadClaimsFromFile(&ca, CLAIMS_FILE);

    /* Ensure at least one admin account exists */
    int adminExists = 0;
    for (int i = 0; i < ua.count; i++) {
        if (ua.users[i].role == ADMIN) { adminExists = 1; break; }
    }
    if (!adminExists) {
        struct User admin;
        strcpy(admin.username,  "admin");
        strcpy(admin.password,  "Admin!123");
        admin.role            = ADMIN;
        admin.last_login      = time(NULL);
        admin.lockoutTime     = 0;
        admin.failed_attempts = 0;
        admin.is_locked       = 0;
        addUser(&ua, admin);
        printf("Default admin created.  Username: admin   Password: Admin!123\n");
        saveUsersToFile(&ua, USERS_FILE);
    }

    int  choice, claim_id;
    int  logged_in = 0;
    int  user_index = -1;
    char username[MAX_USERNAME_LENGTH];
    char password[MAX_PASSWORD_LENGTH];

    while (1) {

        /* ---- NOT LOGGED IN ---- */
        if (!logged_in) {
            printAsterics(30);
            printf("  CLAIMS MANAGEMENT SYSTEM\n");
            printAsterics(30);
            printf("\n1. Login\n2. Sign Up\n3. Exit\n\nChoice: ");
            if (scanf("%d", &choice) != 1) { getchar(); continue; }
            getchar();
            padding(1);

            switch (choice) {
                case 1:
                    printLine(30);
                    printf("Username: ");
                    fgets(username, MAX_USERNAME_LENGTH, stdin);
                    username[strcspn(username, "\n")] = 0;
                    printf("Password: ");
                    fgets(password, MAX_PASSWORD_LENGTH, stdin);
                    password[strcspn(password, "\n")] = 0;
                    printLine(30);

                    user_index = authenticateUser(&ua, username, password);
                    if (user_index != -1) {
                        logged_in = 1;
                        ua.users[user_index].last_login = time(NULL);
                        saveUsersToFile(&ua, USERS_FILE);
                        padding(1);
                    }
                    break;

                case 2:
                    if (registerUser(&ua)) {
                        printf("Registration successful!\n");
                        saveUsersToFile(&ua, USERS_FILE);
                    }
                    break;

                case 3:
                    goto cleanup;

                default:
                    printf("Invalid choice.\n");
            }

        /* ---- LOGGED IN ---- */
        } else {
            if (!isSessionValid(ua.users[user_index].last_login)) {
                printf("Session expired. Please login again.\n");
                logged_in = 0;
                continue;
            }

            enum UserRole role = ua.users[user_index].role;

            printLine(30);
            printf("\n1. Submit Claim\n2. View My Claims\n");
            if (role != CLIENT) printf("3. Search Claims\n4. Manage a Claim\n");
            if (role == ADMIN)  printf("5. Generate Statistics\n6. Generate Daily Report\n7. Manage Users\n");
            printf("0. Logout\n\nChoice: ");
            printLine(10);
            if (scanf("%d", &choice) != 1) { getchar(); continue; }
            getchar();

            switch (choice) {
                case 1:
                    submitClaim(&ca, ua.users[user_index].username);
                    saveClaimsToFile(&ca, CLAIMS_FILE);
                    break;

                case 2:
                    displayUserClaims(&ca, ua.users[user_index].username, role);
                    break;

                case 3:
                    if (role != CLIENT) {
                        searchClaims(&ca, role);
                    } else {
                        printf("Access denied.\n");
                    }
                    break;

                case 4:
                    if (role != CLIENT) {
                        printf("Enter claim ID to manage: ");
                        if (scanf("%d", &claim_id) == 1) {
                            getchar();
                            manageClaims(&ca, ua.users[user_index].username, role, claim_id);
                            saveClaimsToFile(&ca, CLAIMS_FILE);
                        } else {
                            getchar();
                            printf("Invalid ID.\n");
                        }
                    } else {
                        printf("Access denied.\n");
                    }
                    break;

                case 5:
                    if (role == ADMIN) {
                        generateStatistics(&ca, STATISTICS_FILE);
                    } else {
                        printf("Access denied.\n");
                    }
                    break;

                case 6:
                    if (role == ADMIN) {
                        generateDailyReport(&ca, DAILY_REPORT_FILE);
                    } else {
                        printf("Access denied.\n");
                    }
                    break;

                case 7:
                    if (role == ADMIN) {
                        manageUsers(&ua);
                        saveUsersToFile(&ua, USERS_FILE);
                    } else {
                        printf("Access denied.\n");
                    }
                    break;

                case 0:
                    logged_in = 0;
                    ua.users[user_index].last_login = 0;
                    printf("Logged out successfully.\n");
                    padding(1);
                    break;

                default:
                    printf("Invalid choice.\n");
            }
        }
    }

cleanup:
    saveUsersToFile(&ua, USERS_FILE);
    saveClaimsToFile(&ca, CLAIMS_FILE);
    free(ua.users);
    free(ca.claims);
    return 0;
}

/* ============================================================
 * ARRAY MANAGEMENT
 * ============================================================ */

void initUserArray(struct UserArray *ua) {
    ua->capacity = 10;
    ua->count    = 0;
    ua->users    = malloc(ua->capacity * sizeof(struct User));
}

void initClaimArray(struct ClaimArray *ca) {
    ca->capacity = 10;
    ca->count    = 0;
    ca->claims   = malloc(ca->capacity * sizeof(struct Claim));
}

/**
 * addUser — inserts a user, rejecting duplicate usernames.
 * Doubles the array capacity when full.
 */
void addUser(struct UserArray *ua, struct User user) {
    for (int i = 0; i < ua->count; i++) {
        if (strcmp(ua->users[i].username, user.username) == 0) {
            printf("Username '%s' already exists.\n", user.username);
            return;
        }
    }
    if (ua->count == ua->capacity) {
        ua->capacity *= 2;
        ua->users = realloc(ua->users, ua->capacity * sizeof(struct User));
    }
    ua->users[ua->count++] = user;
}

/**
 * addClaim — appends a claim, doubling capacity when full.
 */
void addClaim(struct ClaimArray *ca, struct Claim claim) {
    if (ca->count == ca->capacity) {
        ca->capacity *= 2;
        ca->claims = realloc(ca->claims, ca->capacity * sizeof(struct Claim));
    }
    ca->claims[ca->count++] = claim;
}

/* ============================================================
 * FILE PERSISTENCE
 * ============================================================ */

void saveUsersToFile(struct UserArray *ua, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) { printf("Error: cannot open '%s' for writing.\n", filename); return; }

    fprintf(f, "%d\n", ua->count);
    for (int i = 0; i < ua->count; i++) {
        fprintf(f, "%s,%s,%d,%ld,%ld,%d,%d\n",
            ua->users[i].username,
            ua->users[i].password,
            (int)ua->users[i].role,
            (long)ua->users[i].last_login,
            (long)ua->users[i].lockoutTime,   /* field added — fixes lockout persistence */
            ua->users[i].failed_attempts,
            ua->users[i].is_locked);
    }
    fclose(f);
}

void loadUsersFromFile(struct UserArray *ua, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return;

    int count;
    if (fscanf(f, "%d\n", &count) != 1) { fclose(f); return; }

    for (int i = 0; i < count; i++) {
        struct User user;
        int  role, failed, locked;
        long last_login, lockoutTime;

        /* Read all 7 fields (including lockoutTime) */
        int fields = fscanf(f, "%49[^,],%49[^,],%d,%ld,%ld,%d,%d\n",
            user.username, user.password, &role,
            &last_login, &lockoutTime, &failed, &locked);

        if (fields == 7) {
            user.role            = (enum UserRole)role;
            user.last_login      = (time_t)last_login;
            user.lockoutTime     = (time_t)lockoutTime;
            user.failed_attempts = failed;
            user.is_locked       = locked;
            addUser(ua, user);
        } else {
            printf("Warning: skipping malformed user record.\n");
        }
    }
    fclose(f);
}

void saveClaimsToFile(struct ClaimArray *ca, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) { printf("Error: cannot open '%s' for writing.\n", filename); return; }

    fprintf(f, "%d\n", ca->count);
    for (int i = 0; i < ca->count; i++) {
        if (fprintf(f, "%d,%s,%s,%d,%s,%d,%d,%ld,%ld,%s\n",
                ca->claims[i].id,
                ca->claims[i].username,
                ca->claims[i].description,
                (int)ca->claims[i].category,
                ca->claims[i].reason,
                (int)ca->claims[i].status,
                (int)ca->claims[i].priority,
                (long)ca->claims[i].submission_date,
                (long)ca->claims[i].last_status_change,
                ca->claims[i].resolution_note) < 0) {
            printf("Error writing claim %d to file.\n", ca->claims[i].id);
            fclose(f);
            return;
        }
    }
    fclose(f);
    printf("Claims saved successfully.\n");
}

void loadClaimsFromFile(struct ClaimArray *ca, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return;

    int count;
    if (fscanf(f, "%d\n", &count) != 1) {
        printf("Error reading claim count from file.\n");
        fclose(f);
        return;
    }

    for (int i = 0; i < count; i++) {
        struct Claim claim;
        int  category, status, priority;
        long submission_date, last_status_change;

        int fields = fscanf(f, "%d,%49[^,],%499[^,],%d,%99[^,],%d,%d,%ld,%ld,%499[^\n]\n",
            &claim.id,
            claim.username,
            claim.description,
            &category,
            claim.reason,
            &status,
            &priority,
            &submission_date,
            &last_status_change,
            claim.resolution_note);

        if (fields == 10) {
            claim.category           = (enum ClaimCategory)category;
            claim.status             = (enum ClaimStatus)status;
            claim.priority           = (enum ClaimPriority)priority;
            claim.submission_date    = (time_t)submission_date;
            claim.last_status_change = (time_t)last_status_change;
            addClaim(ca, claim);
        } else {
            printf("Warning: skipping malformed claim record.\n");
        }
    }
    fclose(f);
    printf("Claims loaded successfully.\n");
}

/* ============================================================
 * AUTHENTICATION
 * ============================================================ */

/**
 * authenticateUser — looks up username, checks lockout, validates password.
 * Returns the user's index in ua->users on success, or -1 on failure.
 *
 * Lockout algorithm:
 *   - 3 consecutive wrong passwords → account locked for LOCKOUT_SECONDS.
 *   - On the next login attempt after the lockout period, the lock resets.
 */
int authenticateUser(struct UserArray *ua, const char *username, const char *password) {
    for (int i = 0; i < ua->count; i++) {
        if (strcmp(ua->users[i].username, username) != 0) continue;

        /* Check lockout status */
        if (ua->users[i].is_locked) {
            double elapsed = difftime(time(NULL), ua->users[i].lockoutTime);
            if (elapsed < LOCKOUT_SECONDS) {
                int remaining = (int)((LOCKOUT_SECONDS - elapsed) / 60) + 1;
                printf("Account is locked. Try again in ~%d minute(s).\n", remaining);
                return -1;
            }
            /* Lockout expired — automatically reset */
            ua->users[i].is_locked       = 0;
            ua->users[i].failed_attempts = 0;
        }

        if (strcmp(ua->users[i].password, password) == 0) {
            ua->users[i].failed_attempts = 0;
            printf("Welcome, %s!\n", username);
            return i;
        }

        /* Wrong password */
        ua->users[i].failed_attempts++;
        if (ua->users[i].failed_attempts >= MAX_LOGIN_ATTEMPTS) {
            ua->users[i].lockoutTime = time(NULL);
            ua->users[i].is_locked   = 1;
            printf("Account locked after %d failed attempts. Try again in 30 minutes.\n",
                   MAX_LOGIN_ATTEMPTS);
        } else {
            printf("Incorrect password. %d attempt(s) remaining.\n",
                   MAX_LOGIN_ATTEMPTS - ua->users[i].failed_attempts);
        }
        return -1;
    }
    printf("User '%s' not found.\n", username);
    return -1;
}

/** Returns 1 if the session started within SESSION_SECONDS ago. */
int isSessionValid(time_t lastLogin) {
    return difftime(time(NULL), lastLogin) <= SESSION_SECONDS;
}

/**
 * registerUser — prompts for username + password and creates a CLIENT account.
 *
 * Password policy (enforced by isPasswordValid):
 *   - Minimum 8 characters
 *   - At least one uppercase letter
 *   - At least one lowercase letter
 *   - At least one digit
 *   - At least one special character from: !@#$%^&*
 *   - Must not contain the username as a substring
 */
int registerUser(struct UserArray *ua) {
    struct User newUser;

    printf("Enter username: ");
    fgets(newUser.username, MAX_USERNAME_LENGTH, stdin);
    newUser.username[strcspn(newUser.username, "\n")] = 0;

    char password[MAX_PASSWORD_LENGTH];
    do {
        printf("Enter password: ");
        fgets(password, MAX_PASSWORD_LENGTH, stdin);
        password[strcspn(password, "\n")] = 0;
        if (!isPasswordValid(password, newUser.username)) {
            printf("Password must be >=8 chars and contain uppercase, lowercase, digit, and special char (!@#$%%^&*).\n");
        }
    } while (!isPasswordValid(password, newUser.username));

    strcpy(newUser.password, password);
    newUser.role            = CLIENT;
    newUser.last_login      = time(NULL);
    newUser.lockoutTime     = 0;
    newUser.failed_attempts = 0;
    newUser.is_locked       = 0;

    addUser(ua, newUser);
    return 1;
}

/** Returns 1 if the password satisfies the security policy. */
int isPasswordValid(const char *password, const char *username) {
    int len         = (int)strlen(password);
    int has_upper   = 0, has_lower = 0, has_digit = 0, has_special = 0;

    if (len < 8)                             return 0;
    if (strstr(password, username) != NULL)  return 0;

    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)password[i];
        if      (isupper(c))              has_upper   = 1;
        else if (islower(c))              has_lower   = 1;
        else if (isdigit(c))              has_digit   = 1;
        else if (strchr("!@#$%^&*", c))   has_special = 1;
    }
    return has_upper && has_lower && has_digit && has_special;
}

/* ============================================================
 * DATE HELPERS
 * ============================================================ */

/** Formats a time_t as "YYYY-MM-DD HH:MM:SS" into dateStr. */
void formatDate(time_t timestamp, char *dateStr) {
    struct tm *tm_info = localtime(&timestamp);
    if (tm_info == NULL) {
        strcpy(dateStr, "N/A");
        return;
    }
    strftime(dateStr, DATE_STRING_LENGTH, "%Y-%m-%d %H:%M:%S", tm_info);
}

/** Parses a "YYYY-MM-DD" string back to a time_t. */
time_t parseDate(const char *dateStr) {
    struct tm tm_info = {0};
    sscanf(dateStr, "%d-%d-%d",
           &tm_info.tm_year, &tm_info.tm_mon, &tm_info.tm_mday);
    tm_info.tm_year -= 1900;
    tm_info.tm_mon  -= 1;
    return mktime(&tm_info);
}

/* ============================================================
 * PRIORITY INFERENCE
 *
 * Algorithm: scan the description (case-insensitive) for trigger words.
 *
 *   HIGH   triggers : urgent, emergency, critical, severe
 *   MEDIUM triggers : important, significant, moderate
 *   LOW    (default): anything else
 *
 * Used by both submitClaim() and manageClaims() to keep the logic
 * in a single place.
 * ============================================================ */

enum ClaimPriority inferPriorityFromDescription(const char *description) {
    char *lower = my_strdup(description);
    if (lower == NULL) return LOW;

    for (int i = 0; lower[i]; i++)
        lower[i] = (char)tolower((unsigned char)lower[i]);

    enum ClaimPriority priority;

    if (strstr(lower, "urgent")    || strstr(lower, "emergency") ||
        strstr(lower, "critical")  || strstr(lower, "severe")) {
        priority = HIGH;
    } else if (strstr(lower, "important") || strstr(lower, "significant") ||
               strstr(lower, "moderate")) {
        priority = MEDIUM;
    } else {
        priority = LOW;
    }

    free(lower);
    return priority;
}

/* ============================================================
 * CLAIM SUBMISSION & DISPLAY
 * ============================================================ */

/**
 * submitClaim — collects description, reason, and category from the user,
 * then infers priority automatically from the description text.
 */
void submitClaim(struct ClaimArray *ca, const char *username) {
    struct Claim claim;
    claim.id                 = ca->count + 1;
    claim.resolution_note[0] = '\0';
    strcpy(claim.username, username);

    printAsterics(30);
    printf("Enter claim description: ");
    fgets(claim.description, MAX_DESCRIPTION_LENGTH, stdin);
    claim.description[strcspn(claim.description, "\n")] = 0;

    printf("Enter claim reason: ");
    fgets(claim.reason, MAX_REASON_LENGTH, stdin);
    claim.reason[strcspn(claim.reason, "\n")] = 0;

    printAsterics(30);
    printf("Select category:\n  1. Payment\n  2. Customer Services\n  3. Technical\nChoice: ");
    int cat;
    if (scanf("%d", &cat) != 1 || cat < 1 || cat > 3) cat = 3;
    getchar();
    claim.category = (enum ClaimCategory)(cat - 1);

    claim.status             = PENDING;
    claim.submission_date    = time(NULL);
    claim.last_status_change = claim.submission_date;
    claim.priority           = inferPriorityFromDescription(claim.description);

    addClaim(ca, claim);
    printf("Claim #%d submitted. Auto-assigned priority: %s\n",
           claim.id, getPriorityString(claim.priority));
}

/**
 * compareClaims — qsort comparator: sorts HIGH → MEDIUM → LOW,
 * breaking ties by newest submission date first.
 */
int compareClaims(const void *a, const void *b) {
    const struct Claim *ca = (const struct Claim *)a;
    const struct Claim *cb = (const struct Claim *)b;

    if (ca->priority != cb->priority)
        return (int)cb->priority - (int)ca->priority;

    return (int)difftime(cb->submission_date, ca->submission_date);
}

/**
 * displayUserClaims — shows claims sorted by priority (highest first).
 * Admins and agents see ALL claims; clients see only their own.
 * After display, a client may choose to modify or delete a claim.
 */
void displayUserClaims(struct ClaimArray *ca, const char *username, enum UserRole role) {
    qsort(ca->claims, ca->count, sizeof(struct Claim), compareClaims);

    printf("Claims List:\n");
    printAsterics(100);
    printf("%-5s %-18s %-12s %-10s %-22s %-20s\n",
           "ID", "Category", "Status", "Priority", "Date", "Username");
    printAsterics(100);

    int found = 0;
    for (int i = 0; i < ca->count; i++) {
        /* Access control: clients see only their own claims */
        if (role == CLIENT && strcmp(ca->claims[i].username, username) != 0) continue;

        char dateStr[DATE_STRING_LENGTH];
        formatDate(ca->claims[i].submission_date, dateStr);

        printf("%-5d %-18s %-12s %-10s %-22s %-20s\n",
            ca->claims[i].id,
            getCategoryString(ca->claims[i].category),
            getStatusString(ca->claims[i].status),
            getPriorityString(ca->claims[i].priority),
            dateStr,
            ca->claims[i].username);
        printf("  Description : %s\n", ca->claims[i].description);
        printf("  Reason      : %s\n", ca->claims[i].reason);

        if (ca->claims[i].status != PENDING &&
            strlen(ca->claims[i].resolution_note) > 0) {
            printf("  Resolution  : %s\n", ca->claims[i].resolution_note);
        }
        printAsterics(100);
        printf("\n");
        found = 1;
    }

    if (!found) printf("No claims to display.\n");

    /* Clients can modify/delete their claims (within 24 hours) */
    if (role == CLIENT) {
        int claim_id;
        printf("Enter claim ID to modify/delete (0 to go back): ");
        if (scanf("%d", &claim_id) == 1 && claim_id != 0) {
            getchar();
            manageClaims(ca, username, role, claim_id);
        } else {
            getchar();
        }
    }
}

/* ============================================================
 * SEARCH
 * ============================================================ */

/**
 * searchClaims — filters claims by a chosen field and search term.
 * Available only to ADMIN and CLAIM_AGENT (enforced in main).
 */
void searchClaims(struct ClaimArray *ca, enum UserRole role) {
    (void)role; /* access control already enforced by caller */

    if (ca->count == 0) { printf("No claims to search.\n"); return; }

    int choice;
    printf("Search by:\n"
           "  1. ID\n  2. Username\n  3. Category\n"
           "  4. Status\n  5. Priority\n  6. Submission Date\n"
           "Choice: ");
    if (scanf("%d", &choice) != 1 || choice < 1 || choice > 6) {
        getchar();
        printf("Invalid search option.\n");
        return;
    }
    getchar();

    char searchTerm[MAX_DESCRIPTION_LENGTH];
    printf("Enter search term: ");
    fgets(searchTerm, MAX_DESCRIPTION_LENGTH, stdin);
    searchTerm[strcspn(searchTerm, "\n")] = 0;

    if (strlen(searchTerm) == 0) { printf("Please enter a search term.\n"); return; }

    int    found   = 0;
    char   dateStr[DATE_STRING_LENGTH];

    printf("Search results:\n");
    printAsterics(100);

    for (int i = 0; i < ca->count; i++) {
        int match = 0;

        switch (choice) {
            case 1: match = (ca->claims[i].id == atoi(searchTerm)); break;
            case 2: match = (my_strcasecmp(ca->claims[i].username, searchTerm) == 0); break;
            case 3: match = (my_strcasecmp(getCategoryString(ca->claims[i].category), searchTerm) == 0); break;
            case 4:
                match = ((my_strcasecmp(searchTerm, "pending")     == 0 && ca->claims[i].status == PENDING)     ||
                         (my_strcasecmp(searchTerm, "in progress")  == 0 && ca->claims[i].status == IN_PROGRESS) ||
                         (my_strcasecmp(searchTerm, "resolved")     == 0 && ca->claims[i].status == RESOLVED)    ||
                         (my_strcasecmp(searchTerm, "rejected")     == 0 && ca->claims[i].status == REJECTED));
                break;
            case 5:
                match = ((my_strcasecmp(searchTerm, "high")   == 0 && ca->claims[i].priority == HIGH)   ||
                         (my_strcasecmp(searchTerm, "medium")  == 0 && ca->claims[i].priority == MEDIUM) ||
                         (my_strcasecmp(searchTerm, "low")     == 0 && ca->claims[i].priority == LOW));
                break;
            case 6:
                formatDate(ca->claims[i].submission_date, dateStr);
                match = (strstr(dateStr, searchTerm) != NULL);
                break;
        }

        if (match) {
            found = 1;
            formatDate(ca->claims[i].submission_date, dateStr);
            printf("ID: %-4d | User: %-15s | Cat: %-18s | Status: %-12s | Priority: %-8s | %s\n",
                ca->claims[i].id,
                ca->claims[i].username,
                getCategoryString(ca->claims[i].category),
                getStatusString(ca->claims[i].status),
                getPriorityString(ca->claims[i].priority),
                dateStr);
            printf("  Description: %s\n", ca->claims[i].description);
            printAsterics(100);
        }
    }

    if (!found) printf("No matching claims found.\n");
}

/* ============================================================
 * CLAIM MANAGEMENT
 * ============================================================ */

/**
 * manageClaims — allows authorised users to modify or delete a claim.
 *
 * Permissions:
 *   CLIENT     : edit description or delete — only within 24 hours, only own claims
 *   CLAIM_AGENT: change status (In Progress / Resolved / Rejected), edit description
 *   ADMIN      : all of the above plus force-delete any claim
 *
 * Priority is automatically re-inferred after a description edit.
 */
void manageClaims(struct ClaimArray *ca, const char *username, enum UserRole role, int claim_id) {
    for (int i = 0; i < ca->count; i++) {
        if (ca->claims[i].id != claim_id) continue;

        /* Authorisation */
        if (role == CLIENT && strcmp(ca->claims[i].username, username) != 0) {
            printf("You are not authorised to modify this claim.\n");
            return;
        }
        if (role == CLIENT && !isWithin24Hours(ca->claims[i].submission_date)) {
            printf("You can no longer modify this claim (24-hour window has passed).\n");
            return;
        }

        printf("Status   : %s\n", getStatusString(ca->claims[i].status));
        printf("Priority : %s\n", getPriorityString(ca->claims[i].priority));

        int choice;
        if (role == CLIENT) {
            printf("1. Edit Description\n2. Delete Claim\nChoice: ");
        } else {
            printf("1. Edit Description\n"
                   "2. Mark as In Progress\n"
                   "3. Mark as Resolved\n"
                   "4. Mark as Rejected\n"
                   "5. Delete Claim\n"
                   "Choice: ");
        }
        if (scanf("%d", &choice) != 1) { getchar(); return; }
        getchar();

        switch (choice) {
            case 1:
                printf("Enter new description: ");
                fgets(ca->claims[i].description, MAX_DESCRIPTION_LENGTH, stdin);
                ca->claims[i].description[strcspn(ca->claims[i].description, "\n")] = 0;
                /* Re-infer priority from updated description */
                ca->claims[i].priority = inferPriorityFromDescription(ca->claims[i].description);
                printf("Description updated. New priority: %s\n",
                       getPriorityString(ca->claims[i].priority));
                break;

            case 2:
                if (role == CLIENT) {
                    /* Client delete: shift array left */
                    for (int j = i; j < ca->count - 1; j++)
                        ca->claims[j] = ca->claims[j + 1];
                    ca->count--;
                    printf("Claim deleted successfully.\n");
                    return;
                }
                /* Agent/Admin: mark In Progress */
                ca->claims[i].status             = IN_PROGRESS;
                ca->claims[i].last_status_change = time(NULL);
                printf("Enter resolution note: ");
                fgets(ca->claims[i].resolution_note, MAX_DESCRIPTION_LENGTH, stdin);
                ca->claims[i].resolution_note[strcspn(ca->claims[i].resolution_note, "\n")] = 0;
                printf("Status updated to In Progress.\n");
                break;

            case 3:
                if (role == CLIENT) { printf("Access denied.\n"); return; }
                ca->claims[i].status             = RESOLVED;
                ca->claims[i].last_status_change = time(NULL);
                printf("Enter resolution note: ");
                fgets(ca->claims[i].resolution_note, MAX_DESCRIPTION_LENGTH, stdin);
                ca->claims[i].resolution_note[strcspn(ca->claims[i].resolution_note, "\n")] = 0;
                printf("Status updated to Resolved.\n");
                break;

            case 4:
                if (role == CLIENT) { printf("Access denied.\n"); return; }
                ca->claims[i].status             = REJECTED;
                ca->claims[i].last_status_change = time(NULL);
                printf("Enter resolution note: ");
                fgets(ca->claims[i].resolution_note, MAX_DESCRIPTION_LENGTH, stdin);
                ca->claims[i].resolution_note[strcspn(ca->claims[i].resolution_note, "\n")] = 0;
                printf("Status updated to Rejected.\n");
                break;

            case 5:
                if (role != ADMIN) { printf("Only admins can force-delete a claim.\n"); return; }
                for (int j = i; j < ca->count - 1; j++)
                    ca->claims[j] = ca->claims[j + 1];
                ca->count--;
                printf("Claim deleted successfully.\n");
                return;

            default:
                printf("Invalid choice.\n");
                return;
        }
        printf("Claim updated successfully.\n");
        return;
    }
    printf("Claim #%d not found.\n", claim_id);
}

/* ============================================================
 * USER MANAGEMENT  (Admin only)
 * ============================================================ */

/**
 * manageUsers — allows the admin to change a user's role or delete them.
 */
void manageUsers(struct UserArray *ua) {
    char username[MAX_USERNAME_LENGTH];
    printf("Enter username to manage: ");
    fgets(username, MAX_USERNAME_LENGTH, stdin);
    username[strcspn(username, "\n")] = 0;

    for (int i = 0; i < ua->count; i++) {
        if (strcmp(ua->users[i].username, username) != 0) continue;

        const char *roleStr =
            ua->users[i].role == ADMIN       ? "Admin" :
            ua->users[i].role == CLAIM_AGENT ? "Claim Agent" : "Client";
        printf("Current role: %s\n", roleStr);
        printf("1. Change to Admin\n2. Change to Claim Agent\n3. Change to Client\n4. Delete User\nChoice: ");

        int choice;
        if (scanf("%d", &choice) != 1) { getchar(); return; }
        getchar();

        switch (choice) {
            case 1: ua->users[i].role = ADMIN;       break;
            case 2: ua->users[i].role = CLAIM_AGENT; break;
            case 3: ua->users[i].role = CLIENT;      break;
            case 4:
                for (int j = i; j < ua->count - 1; j++)
                    ua->users[j] = ua->users[j + 1];
                ua->count--;
                printf("User '%s' deleted.\n", username);
                return;
            default:
                printf("Invalid choice.\n");
                return;
        }
        printf("Role updated successfully.\n");
        return;
    }
    printf("User '%s' not found.\n", username);
}

/* ============================================================
 * STATISTICS & REPORTS  (Admin only)
 * ============================================================ */

/**
 * generateStatistics — prints a summary to the console and saves it to filename.
 *
 * Uses enum values directly as array indices (no off-by-one arithmetic):
 *   priorityCounts[LOW=0], [MEDIUM=1], [HIGH=2]
 *   statusCounts[PENDING=0], [IN_PROGRESS=1], [RESOLVED=2], [REJECTED=3]
 *   categoryCounts[PAYMENT=0], [CUSTOMER_SERVICES=1], [TECHNICAL=2]
 */
void generateStatistics(struct ClaimArray *ca, const char *filename) {
    if (ca->count == 0) {
        printf("No claims available to generate statistics.\n");
        return;
    }

    int total = ca->count;

    int priorityCounts[3]  = {0};   /* LOW, MEDIUM, HIGH */
    int statusCounts[4]    = {0};   /* PENDING, IN_PROGRESS, RESOLVED, REJECTED */
    int categoryCounts[3]  = {0};   /* PAYMENT, CUSTOMER_SERVICES, TECHNICAL */

    for (int i = 0; i < ca->count; i++) {
        priorityCounts[(int)ca->claims[i].priority]++;
        statusCounts[(int)ca->claims[i].status]++;
        categoryCounts[(int)ca->claims[i].category]++;
    }

    const enum ClaimPriority  priorities[]  = {LOW, MEDIUM, HIGH};
    const enum ClaimStatus    statuses[]    = {PENDING, IN_PROGRESS, RESOLVED, REJECTED};
    const enum ClaimCategory  categories[]  = {PAYMENT, CUSTOMER_SERVICES, TECHNICAL};

    /* ---- Console output ---- */
    printf("===== STATISTICS =====\nTotal Claims: %d\n\n", total);

    printf("--- Priority ---\n");
    for (int i = 0; i < 3; i++) {
        double pct = (double)priorityCounts[i] / total * 100.0;
        printf("  %-8s: %d (%.1f%%)\n", getPriorityString(priorities[i]), priorityCounts[i], pct);
    }

    printf("\n--- Status ---\n");
    for (int i = 0; i < 4; i++) {
        double pct = (double)statusCounts[i] / total * 100.0;
        printf("  %-12s: %d (%.1f%%)\n", getStatusString(statuses[i]), statusCounts[i], pct);
    }

    printf("\n--- Category ---\n");
    for (int i = 0; i < 3; i++) {
        double pct = (double)categoryCounts[i] / total * 100.0;
        printf("  %-18s: %d (%.1f%%)\n", getCategoryString(categories[i]), categoryCounts[i], pct);
    }

    /* ---- File output ---- */
    FILE *f = fopen(filename, "w");
    if (!f) { printf("Warning: could not save statistics to file.\n"); return; }

    char dateStr[DATE_STRING_LENGTH];
    formatDate(time(NULL), dateStr);
    fprintf(f, "Statistics Report — %s\nTotal Claims: %d\n\n", dateStr, total);

    fprintf(f, "--- Priority ---\n");
    for (int i = 0; i < 3; i++) {
        double pct = (double)priorityCounts[i] / total * 100.0;
        fprintf(f, "  %-8s: %d (%.1f%%)\n", getPriorityString(priorities[i]), priorityCounts[i], pct);
    }
    fprintf(f, "\n--- Status ---\n");
    for (int i = 0; i < 4; i++) {
        double pct = (double)statusCounts[i] / total * 100.0;
        fprintf(f, "  %-12s: %d (%.1f%%)\n", getStatusString(statuses[i]), statusCounts[i], pct);
    }
    fprintf(f, "\n--- Category ---\n");
    for (int i = 0; i < 3; i++) {
        double pct = (double)categoryCounts[i] / total * 100.0;
        fprintf(f, "  %-18s: %d (%.1f%%)\n", getCategoryString(categories[i]), categoryCounts[i], pct);
    }

    fclose(f);
    printf("Statistics saved to '%s'.\n", filename);
}

/**
 * generateDailyReport — writes today's new and resolved claims to a file.
 */
void generateDailyReport(struct ClaimArray *ca, const char *filename) {
    time_t now   = time(NULL);
    struct tm *t = localtime(&now);

    FILE *f = fopen(filename, "w");
    if (!f) { printf("Error opening report file.\n"); return; }

    fprintf(f, "Daily Report — %04d-%02d-%02d\n\n",
        t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);

    fprintf(f, "New Claims (submitted today):\n");
    fprintf(f, "______________________________\n");
    for (int i = 0; i < ca->count; i++) {
        struct tm *sub = localtime(&ca->claims[i].submission_date);
        if (sub->tm_mday == t->tm_mday &&
            sub->tm_mon  == t->tm_mon  &&
            sub->tm_year == t->tm_year) {
            char ds[DATE_STRING_LENGTH];
            strftime(ds, DATE_STRING_LENGTH, "%Y-%m-%d %H:%M:%S", sub);
            fprintf(f, "  #%-4d | %-15s | %-18s | %-8s | %s\n",
                ca->claims[i].id,
                ca->claims[i].username,
                getCategoryString(ca->claims[i].category),
                getPriorityString(ca->claims[i].priority),
                ds);
        }
    }

    fprintf(f, "\nResolved Claims (today):\n");
    fprintf(f, "________________________\n");
    for (int i = 0; i < ca->count; i++) {
        if (ca->claims[i].status != RESOLVED) continue;
        struct tm *chg = localtime(&ca->claims[i].last_status_change);
        if (chg->tm_mday == t->tm_mday &&
            chg->tm_mon  == t->tm_mon  &&
            chg->tm_year == t->tm_year) {
            char ds[DATE_STRING_LENGTH];
            strftime(ds, DATE_STRING_LENGTH, "%Y-%m-%d %H:%M:%S", chg);
            fprintf(f, "  #%-4d | %-15s | %-18s | %-8s | resolved: %s\n",
                ca->claims[i].id,
                ca->claims[i].username,
                getCategoryString(ca->claims[i].category),
                getPriorityString(ca->claims[i].priority),
                ds);
        }
    }

    fclose(f);
    printf("Daily report saved to '%s'.\n", filename);
}

/* ============================================================
 * ENUM → STRING HELPERS
 * ============================================================ */

const char *getCategoryString(enum ClaimCategory category) {
    switch (category) {
        case PAYMENT:           return "Payment";
        case CUSTOMER_SERVICES: return "Customer Service";
        case TECHNICAL:         return "Technical";
        default:                return "Unknown";
    }
}

const char *getStatusString(enum ClaimStatus status) {
    switch (status) {
        case PENDING:     return "Pending";
        case IN_PROGRESS: return "In Progress";
        case RESOLVED:    return "Resolved";
        case REJECTED:    return "Rejected";
        default:          return "Unknown";
    }
}

const char *getPriorityString(enum ClaimPriority priority) {
    switch (priority) {
        case HIGH:   return "High";
        case MEDIUM: return "Medium";
        case LOW:    return "Low";
        default:     return "Unknown";
    }
}

/* ============================================================
 * UTILITY
 * ============================================================ */

/** Returns 1 if submissionTime is within the last 24 hours. */
int isWithin24Hours(time_t submissionTime) {
    return difftime(time(NULL), submissionTime) <= (24.0 * 60.0 * 60.0);
}

void printLine(int num) {
    for (int i = 0; i < num; i++) putchar('_');
    putchar('\n');
}

void printSlashes(int num) {
    for (int i = 0; i < num; i++) putchar('/');
    putchar('\n');
}

void printAsterics(int num) {
    for (int i = 0; i < num; i++) putchar('*');
    putchar('\n');
}

void padding(int num) {
    for (int i = 0; i < num; i++) putchar('\n');
}
