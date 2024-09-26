#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>

// Global Constants

#define MAX_USERNAME_LENGTH 50
#define MAX_PASSWORD_LENGTH 50
#define MAX_DESCRIPTION_LENGTH 500
#define MAX_REASON_LENGTH 100
#define MAX_CATEGORY_LENGTH 100
#define MAX_STATUS_LENGTH 20
#define MAX_FILENAME_LENGTH 100
#define DATE_STRING_LENGTH 11 // YYYY-MM-DD\0


// Files 

#define SESSION_FILE "session.txt"
#define USERS_FILE "users.txt"
#define CLAIMS_FILE "claims.txt"
#define STATISTICS_FILE "statistics.txt"
#define DAILY_REPORT_FILE "daily_report.txt"




enum UserRole {
	ADMIN,
	CLAIM_AGENT,
	CLIENT
};

enum ClaimStatus {
	PENDING,
	IN_PROGRESS,
	RESOLVED,
	REJECTED
};

enum ClaimPriority {
	LOW,
	MEDIUM,
	HIGH
};

enum ClaimCategory {
	SHIPPING,
	PAYMENT,
	CUSTOMER_SERVICES,
	TECHNICAL,
	RETURN
};

struct User {
	char username[MAX_USERNAME_LENGTH];
	char password[MAX_PASSWORD_LENGTH];
	enum UserRole role;
	time_t last_login;
	time_t lockoutTime;
	int failed_attempts;
	int is_locked;
};

struct Claim {
	int id;
	char username[MAX_USERNAME_LENGTH];
	char description[MAX_DESCRIPTION_LENGTH];
	enum ClaimCategory category;
	char reason[MAX_REASON_LENGTH];  
	enum ClaimStatus status;
	enum ClaimPriority priority;
	time_t submission_date;
	time_t last_status_change; 
    char resolution_note[MAX_DESCRIPTION_LENGTH];
};

struct UserArray {
	struct User* users;
	int count;
	int capacity;
};

struct ClaimArray {
	struct Claim* claims;
	int count;
	int capacity;
};

// Function prototypes

//intiate Arrays
void initUserArray(struct UserArray* userArray);
void initClaimArray(struct ClaimArray* claimArray);

void addUser(struct UserArray* userArray, struct User user);
void addClaim(struct ClaimArray* claimArray, struct Claim claim);

// Loading & Saving data from .txt after closing and repopening
void saveUsersToFile(struct UserArray* userArray, const char* filename);
void loadUsersFromFile(struct UserArray* userArray, const char* filename);
void saveClaimsToFile(struct ClaimArray* claimArray, const char* filename);
void loadClaimsFromFile(struct ClaimArray* claimArray, const char* filename);

int authenticateUser(struct UserArray* userArray, const char* username, const char* password);
int isSessionValid(time_t lastLogin);

void updateUserRole(struct UserArray* userArray, const char* username, enum UserRole newRole);
void displayClaimsByPriority(struct ClaimArray* claimArray, const char* username, enum UserRole role);

int registerUser(struct UserArray* userArray);
int isPasswordValid(const char* password, const char* username);

// Function to convert between time_t and YYYY-MM-DD string
void formatDate(time_t timestamp, char* dateStr);
time_t parseDate(const char* dateStr);

//Claim management and display
void submitClaim(struct ClaimArray* claimArray, const char* username);
void displayUserClaims(struct ClaimArray* claimArray, const char* username, enum UserRole role);
void searchClaims(struct ClaimArray* claimArray, enum UserRole role);
void generateStatistics(struct ClaimArray* claimArray, char* filename);
void manageClaims(struct ClaimArray* claimArray, const char* username, enum UserRole role, int claim_id);
int compareClaims(const void* a, const void* b);

//User management
void manageUsers(struct UserArray* userArray);


// Handling enums
const char* getPriorityString(enum ClaimPriority priority);
const char* getCategoryString(enum ClaimCategory category);
const char* getStatusString(enum ClaimStatus status);
int isWithin24Hours(time_t submissionTime);
char* strcasestr_custom(const char* haystack, const char* needle);

// Display
void printLine(int num);
void printSlashes(int num);
void printAsterics(int num);
void padding(int num);
void printTableHeader();
void printTableFooter();
void printTableRow(int id, const char* category, const char* status, const char* priority, const char* username, const char* date);


// Stats
void generateDailyReport(struct ClaimArray* claimArray, const char* filename);


int main() {
	struct UserArray userArray;
	struct ClaimArray claimArray;
	
	initUserArray(&userArray);
	initClaimArray(&claimArray);
	
	// Loading
	loadUsersFromFile(&userArray, USERS_FILE);
	loadClaimsFromFile(&claimArray, CLAIMS_FILE);
	
	int choice, claim_id, logged_in = 0;
	int user_index = -1;
	char username[MAX_USERNAME_LENGTH];
	char password[MAX_PASSWORD_LENGTH];
	
	int adminExists = 0;
	for (int i = 0; i < userArray.count; i++) {
		if (userArray.users[i].role == ADMIN) {
			adminExists = 1;
			break;
		}
	}
	
	if (!adminExists) {
		struct User admin;
		strcpy(admin.username, "admin");
		strcpy(admin.password, "Admin!123");  
		admin.role = ADMIN;
		admin.last_login = time(NULL);
		admin.failed_attempts = 0;
		admin.is_locked = 0;
		addUser(&userArray, admin);
		printf("Default admin user created. Username: admin, Password: Admin123!\n");
		saveUsersToFile(&userArray, USERS_FILE);
	}
	
	while (1) {
		if (!logged_in) {
			printAsterics(25);
			printf("Authentication Menu: \n");
			printAsterics(25);
			printf("\n1. Login\n2. Register\n3. Exit\n\nChoice: ");
			scanf("%d", &choice);
			getchar(); // Consume newline
			padding(2);
			switch (choice) {
				case 1:
					printLine(20);
					printf("Username: ");
					fgets(username, MAX_USERNAME_LENGTH, stdin);
					username[strcspn(username, "\n")] = 0;
					printf("Password: ");
					fgets(password, MAX_PASSWORD_LENGTH, stdin);
					password[strcspn(password, "\n")] = 0;
					printLine(20);
					
					user_index = authenticateUser(&userArray, username, password);
					if (user_index != -1) {
						logged_in = 1;
						printf("Login successful!");
						userArray.users[user_index].last_login = time(NULL); // Update last login time

						padding(2);
					} else {
						printf("Login failed.\n");
					}
					break;
				case 2:
					if (registerUser(&userArray)) {
						printf("Registration successful!\n");
						saveUsersToFile(&userArray, USERS_FILE);
					}
					break;
				case 3:
					goto cleanup;
				default:
					printf("Invalid choice.\n");
			}
		} else {
			if (!isSessionValid(userArray.users[user_index].last_login)) {
				printf("Session expired. Please login again.\n");
				logged_in = 0;
				continue;
			}
			printLine(20);
			printf("\n1. Submit Claim\n2. View Claims\n");
            if (userArray.users[user_index].role != CLIENT) {
                printf("3. Search Claims\n4. Manage Claims\n");
            }
            if (userArray.users[user_index].role == ADMIN) {
                printf("5. Generate Statistics\n6. Generate daily report\n7. Manage Users\n");
            }
			printf("0. Logout\n\nChoice: ");
			printLine(10);
			scanf("%d", &choice);
			getchar(); // Consume newline
			
			switch (choice) {
                case 1:
                    submitClaim(&claimArray, userArray.users[user_index].username);
                    saveClaimsToFile(&claimArray, CLAIMS_FILE);
                    break;
                case 2:
                    displayUserClaims(&claimArray, userArray.users[user_index].username, userArray.users[user_index].role);
					
					// if (userArray.users[user_index].role == CLIENT)
                    // 	displayUserClaims(&claimArray, userArray.users[user_index].username, userArray.users[user_index].role);
					// else {
					// 	displayClaimsByPriority(&claimArray, userArray.users[user_index].username, userArray.users[user_index].role);
					// }
                    break;
                case 3:
                    if (userArray.users[user_index].role != CLIENT) {
                        searchClaims(&claimArray, userArray.users[user_index].role);
                    } else {
                        printf("Invalid choice.\n");
                    }
                    break;
                case 4:
					printf("Enter claim ID to manage: ");
					scanf("%d", &claim_id);
                    if (userArray.users[user_index].role != CLIENT) {
                        manageClaims(&claimArray, userArray.users[user_index].username, userArray.users[user_index].role, claim_id);
                        saveClaimsToFile(&claimArray, CLAIMS_FILE);
                    } else {
                        printf("Invalid choice.\n");
                    }
                    break;
				case 5:
					if (userArray.users[user_index].role != CLIENT) {
						generateStatistics(&claimArray, STATISTICS_FILE);
					} else {
						printf("Access denied.\n");
					}
					break;
				case 6:
					if (userArray.users[user_index].role == ADMIN) {
						generateDailyReport(&claimArray, DAILY_REPORT_FILE);
					}
					break;
				case 7:
					if (userArray.users[user_index].role == ADMIN) {
						manageUsers(&userArray);
						saveUsersToFile(&userArray, USERS_FILE);
					} else {
						printf("Access denied.\n");
					}
					break;
				case 0:
					logged_in = 0;
					printf("Logged out successfully.");
					    userArray.users[user_index].last_login = 0; // Update last login time
					padding(2);
					break;
				default:
					printf("Invalid choice.\n");
			}
		}
	}
	
cleanup:
	saveUsersToFile(&userArray, USERS_FILE);
	saveClaimsToFile(&claimArray, CLAIMS_FILE);
	
	free(userArray.users);
	free(claimArray.claims);
	
	return 0;
}

void initUserArray(struct UserArray* userArray) {
	userArray->capacity = 10;
	userArray->count = 0;
	userArray->users = malloc(userArray->capacity * sizeof(struct User));
}

void initClaimArray(struct ClaimArray* claimArray) {
	claimArray->capacity = 10;
	claimArray->count = 0;
	claimArray->claims = malloc(claimArray->capacity * sizeof(struct Claim));
}

void addUser(struct UserArray* userArray, struct User user) {
    // Check for duplicate usernames
    // for (int i = 0; i < userArray->count; i++) {
    //     if (strcmp(userArray->users[i].username, user.username) == 0) {
    //         printf("Username already exists.\n");
    //         return;
    //     }  
    // }
	if (userArray->count == userArray->capacity) {
		userArray->capacity *= 2;
		userArray->users = realloc(userArray->users, userArray->capacity * sizeof(struct User));
	}
	userArray->users[userArray->count++] = user;
}

void addClaim(struct ClaimArray* claimArray, struct Claim claim) {
	if (claimArray->count == claimArray->capacity) {
		claimArray->capacity *= 2;
		claimArray->claims = realloc(claimArray->claims, claimArray->capacity * sizeof(struct Claim));
	}
	claimArray->claims[claimArray->count++] = claim;
}

// Function to check if an account is locked


// Loading & Saving
void saveUsersToFile(struct UserArray* userArray, const char* filename) {
	FILE* file = fopen(filename, "w");
	if (file == NULL) {
		printf("Error opening file for writing.\n");
		return;
	}
	fprintf(file, "%d\n", userArray->count);
	for (int i = 0; i < userArray->count; i++) {
		fprintf(file, "%s,%s,%d,%ld,%d,%d\n", 
			userArray->users[i].username,
			userArray->users[i].password,
			userArray->users[i].role,
			userArray->users[i].last_login,
			userArray->users[i].failed_attempts,
			userArray->users[i].is_locked);
	}
	fclose(file);
}

void loadUsersFromFile(struct UserArray* userArray, const char* filename) {
	FILE* file = fopen(filename, "r");
	if (file == NULL) {
		printf("No existing user file found. Starting with an empty user list.\n");
		return;
	}
	int count;
	fscanf(file, "%d\n", &count);
	for (int i = 0; i < count; i++) {
		struct User user;
		fscanf(file, "%[^,],%[^,],%d,%ld,%d,%d\n", 
			user.username,
			user.password,
			&user.role,
			&user.last_login,
			&user.failed_attempts,
			&user.is_locked);
		addUser(userArray, user);
	}
	fclose(file);
}

void saveClaimsToFile(struct ClaimArray* claimArray, const char* filename) {
	FILE* file = fopen(filename, "w");
	if (file == NULL) {
		printf("Error opening file for writing.\n");
		return;
	}
	fprintf(file, "%d\n", claimArray->count);
	for (int i = 0; i < claimArray->count; i++) {
		if (fprintf(file, "%d,%s,%s,%d,%s,%d,%d,%ld,%ld,%s\n",
			claimArray->claims[i].id,
			claimArray->claims[i].username,
			claimArray->claims[i].description,
			claimArray->claims[i].category,
			claimArray->claims[i].reason,
			claimArray->claims[i].status,
			claimArray->claims[i].priority,
			claimArray->claims[i].submission_date,
			claimArray->claims[i].last_status_change,
			claimArray->claims[i].resolution_note) < 0) {
				printf("Error writing claim to file.\n");
				fclose(file);
				return;
			}
	}
	fclose(file);
	printf("Claims saved successfully.\n");
}


void loadClaimsFromFile(struct ClaimArray* claimArray, const char* filename) {
	FILE* file = fopen(filename, "r");
	if (file == NULL) {
		printf("No existing claims file found. Starting with an empty claims list.\n");
		return;
	}
	int count;
	if (fscanf(file, "%d\n", &count) != 1) {
		printf("Error reading claim count from file.\n");
		fclose(file);
		return;
	}
	for (int i = 0; i < count; i++) {
		struct Claim claim;
		if (fscanf(file, "%d,%[^,],%[^,],%d,%[^,],%d,%d,%ld,%ld,%[^,]\n",
			&claim.id,
			claim.username,
			claim.description,
			&claim.category,
			claim.reason,
			&claim.status,
			&claim.priority,
			&claim.submission_date,
			&claim.last_status_change,
            claim.resolution_note) != 10) {
				printf("Error reading claim from file. Skipping.\n");
				continue;
			}
		addClaim(claimArray, claim);
	}
	fclose(file);
	printf("Claims loaded successfully.\n");
}

int authenticateUser(struct UserArray* userArray, const char* username, const char* password) {
    for (int i = 0; i < userArray->count; i++) {
        if (strcmp(userArray->users[i].username, username) == 0) {
            if (userArray->users[i].is_locked == 1) {
                time_t currentTime = time(NULL);
                if (currentTime - userArray->users[i].lockoutTime < 1800) { // 1800 seconds = 30 minutes
                    printf("Account is locked. You have attempted to sign in too many times.\n");
                    printf("Please try again after 30 minutes.\n");
                    return -1;
                } else {
                    userArray->users[i].is_locked = 0;
                    userArray->users[i].failed_attempts = 0;
                }
            }

            if (strcmp(userArray->users[i].password, password) == 0) {
                userArray->users[i].failed_attempts = 0;
                printf("Welcome, %s! You have successfully signed in.\n", username);
                return i; // Return the user ID
            } else {
                userArray->users[i].failed_attempts++;
                if (userArray->users[i].failed_attempts >= 3) {
                    userArray->users[i].lockoutTime = time(NULL);
                    userArray->users[i].is_locked = 1;
                    printf("Account locked due to 3 failed sign-in attempts.\n");
                    printf("Please try again after 30 minutes.\n");
                } else {
                    printf("Invalid password. You have %d attempts remaining.\n", 3 - userArray->users[i].failed_attempts);
                }
                return -1; // Authentication failed
            }
        }
    }
    printf("User not found. Please check your username and try again.\n");
    return -1; // User not found
}

int isSessionValid(time_t lastLogin) {
	time_t currentTime = time(NULL);
	return (currentTime - lastLogin) <= 1800;  // 30 minutes = 1800 seconds
}

int registerUser(struct UserArray* userArray) {
	struct User newUser;
	printf("Enter username: ");
	fgets(newUser.username, MAX_USERNAME_LENGTH, stdin);
	newUser.username[strcspn(newUser.username, "\n")] = 0;
	
	// Check if username already exists
	for (int i = 0; i < userArray->count; i++) {
		if (strcmp(userArray->users[i].username, newUser.username) == 0) {
			printf("Username already exists.\n");
			return 0;
		}
	}
	
	char password[MAX_PASSWORD_LENGTH];
	do {
		printf("Enter password: ");
		fgets(password, MAX_PASSWORD_LENGTH, stdin);
		password[strcspn(password, "\n")] = 0;
		
		if (!isPasswordValid(password, newUser.username)) {
			printf("Invalid password. Please try again.\n");
		}
	} while (!isPasswordValid(password, newUser.username));
	
	strcpy(newUser.password, password);
	newUser.role = CLIENT;
	newUser.last_login = time(NULL);
	newUser.failed_attempts = 0;
	newUser.is_locked = 0;
	
	addUser(userArray, newUser);
	return 1;
}

int isPasswordValid(const char* password, const char* username) {
	int len = strlen(password);
	int has_upper = 0, has_lower = 0, has_digit = 0, has_special = 0, has_username;
	
	if (len < 8) return 0;
	if (strstr(password, username) != NULL) return 0;
	
	for (int i = 0; i < len; i++) {
		if (isupper(password[i])) has_upper = 1;
		else if (islower(password[i])) has_lower = 1;
		else if (isdigit(password[i])) has_digit = 1;
		else if (strchr("!@#$%^&*", password[i])) has_special = 1;
	}
	return has_upper && has_lower && has_digit && has_special;
}

// Function to convert time_t to YYYY-MM-DD string
// 

void formatDate(time_t timestamp, char* dateStr) {
    struct tm* tm = localtime(&timestamp);
    if (tm == NULL) {
        // Handle error: unable to convert timestamp to local time
        strcpy(dateStr, "Error: unable to format date");
        return;
    }
    strftime(dateStr, 20, "%Y-%m-%d %H:%M:%S", tm);
}

// Function to convert YYYY-MM-DD string to time_t
time_t parseDate(const char* dateStr) {
	struct tm tm_info = {0};
	sscanf(dateStr, "%d-%d-%d", &tm_info.tm_year, &tm_info.tm_mon, &tm_info.tm_mday);
	tm_info.tm_year -= 1900;  // Adjust year
	tm_info.tm_mon -= 1;      // Adjust month
	return mktime(&tm_info);
}

void submitClaim(struct ClaimArray* claimArray, const char* username) {
	struct Claim newClaim;
	newClaim.id = claimArray->count + 1;
	strcpy(newClaim.username, username);
	
	printAsterics(30);
	printf("Enter claim description: ");
	fgets(newClaim.description, MAX_DESCRIPTION_LENGTH, stdin);
	newClaim.description[strcspn(newClaim.description, "\n")] = 0;
	
	printf("Enter claim reason: ");
	fgets(newClaim.reason, MAX_REASON_LENGTH, stdin);
	newClaim.reason[strcspn(newClaim.reason, "\n")] = 0;
	
	printAsterics(30);
	printf("1. Shipping\n2. Payment\n3. Customer Services\n");
	printf("4. Technical\n5. Return\n");
	int category_choice;
	printf("Select claim category:\n");
	scanf("%d", &category_choice);
	getchar(); // Consume newline
	newClaim.category = (enum ClaimCategory)(category_choice - 1);
	
	newClaim.status = PENDING;
	newClaim.submission_date = time(NULL);
	newClaim.last_status_change = newClaim.submission_date;
	
	// Assign priority based on keywords in description
	char *description_lower = strdup(newClaim.description);
	for (int i = 0; description_lower[i]; i++) {
		description_lower[i] = tolower(description_lower[i]);
	}
	
	if (strstr(description_lower, "urgent") || strstr(description_lower, "emergency") || 
		strstr(description_lower, "critical") || strstr(description_lower, "severe")) {
			newClaim.priority = HIGH;
		} else if (strstr(description_lower, "important") || strstr(description_lower, "significant") || 
			strstr(description_lower, "moderate")) {
				newClaim.priority = MEDIUM;
			} else {
				newClaim.priority = LOW;
			}
	
	free(description_lower);
	
	addClaim(claimArray, newClaim);
	printf("Claim submitted successfully. Priority: %s\n", 
		newClaim.priority == HIGH ? "High" : 
		(newClaim.priority == MEDIUM ? "Medium" : "Low"));
}

int compareClaims(const void* a, const void* b) {
	const struct Claim* claimA = (const struct Claim*)a;
	const struct Claim* claimB = (const struct Claim*)b;
	
	// Sort by priority (HIGH to LOW)
	if (claimA->priority != claimB->priority) {
		return claimB->priority - claimA->priority;
	}
	
	// If priorities are the same, sort by submission date (newest first)
	return difftime(claimB->submission_date, claimA->submission_date);
}

void displayUserClaims(struct ClaimArray* claimArray, const char* username, enum UserRole role) {
    printf("Your Claims List:\n");
    printAsterics(140);
    printf("%-5s %-15s %-10s %-10s %-20s %-20s\n", "ID", "Category", "Status", "Priority", "Date", "Username");
    printAsterics(140);
	// qsort compares every two adjacent elementes using the compareClaims function
	qsort(claimArray->claims, claimArray->count, sizeof(struct Claim), compareClaims);
    for (int i = 0; i < claimArray->count; i++) {
        if (strcmp(claimArray->claims[i].username, username) == 0 || role != CLIENT) {
            char dateStr[20];
            formatDate(claimArray->claims[i].submission_date, dateStr);
            if (strcmp(dateStr, "Error: unable to format date") == 0) {
                printf("Error: unable to format date\n");
                continue;
            }
            printf("%-5d %-15s %-10s %-10s %-20s %-20s\n",
                claimArray->claims[i].id,
                getCategoryString(claimArray->claims[i].category),
                getStatusString(claimArray->claims[i].status),
                getPriorityString(claimArray->claims[i].priority),
                dateStr,
                claimArray->claims[i].username);
            printf("Description: %s\n", claimArray->claims[i].description);
            printf("Reason: %s\n", claimArray->claims[i].reason);

            if (claimArray->claims[i].status != PENDING && strlen(claimArray->claims[i].resolution_note) > 0) {
                printLine(10);
                printf("Resolution Note: %s\n", claimArray->claims[i].resolution_note);
            }
            printAsterics(140);

            if (role == CLIENT && isWithin24Hours(claimArray->claims[i].submission_date)) {
                // printf("Options: 1. Modify 2. Delete\n");
            }
            printf("\n");
        }
    }

    if (role == CLIENT) {
        int choice, claim_id;
        printf("Enter claim ID to modify/delete (0 to exit): ");
        scanf("%d", &claim_id);
        getchar(); // Consume newline

        if (claim_id != 0) {
            manageClaims(claimArray, username, role, claim_id);
        }
    }
}

void displayClaimsByPriority(struct ClaimArray* claimArray, const char* username, enum UserRole role) {
	// Sort the claims array by priority
	qsort(claimArray->claims, claimArray->count, sizeof(struct Claim), compareClaims);
	char dateStr[DATE_STRING_LENGTH];
	if (role == ADMIN || role == CLAIM_AGENT) {
		printf("All claims (sorted by priority):\n");
		for (int i = 0; i < claimArray->count; i++) {
		    formatDate(claimArray->claims[i].submission_date, dateStr);
			printf("ID: %d, Username: %s, Category: %s, Status: %s, Priority: %s, Submission Date: %s\n",
				claimArray->claims[i].id,
				claimArray->claims[i].username,
				getCategoryString(claimArray->claims[i].category),
				getStatusString(claimArray->claims[i].status),
				getPriorityString(claimArray->claims[i].priority),dateStr);
			printf("Description: %s\n", claimArray->claims[i].description);
			printf("Reason: %s\n\n", claimArray->claims[i].reason);
		}
	} else {
		printf("Your claims:\n");
		for (int i = 0; i < claimArray->count; i++) {
			if (strcmp(claimArray->claims[i].username, username) == 0) {
				printf("ID: %d, Category: %s, Status: %s, Priority: %s\n",
					claimArray->claims[i].id,
					getCategoryString(claimArray->claims[i].category),
					getStatusString(claimArray->claims[i].status),
					getPriorityString(claimArray->claims[i].priority));
				printf("Description: %s\n", claimArray->claims[i].description);
				printf("Reason: %s\n\n", claimArray->claims[i].reason);
			}
		}
	}
}


// check if a user is authorized to view a claim (all users can see are their claims but Admin/Agent can see everything)
int isAuthorizedToViewClaim(const struct Claim* claim, const char* username, enum UserRole role) {
	return (role == ADMIN || role == CLAIM_AGENT || strcmp(claim->username, username) == 0);
}


void searchClaims(struct ClaimArray* claimArray, enum UserRole role) {
    int choice;
    char dateStr[DATE_STRING_LENGTH];
    printf("Search by:\n1. ID\n2. Username\n3. Category\n4. Status\n5. Priority\n");
    printf("6. Submission Date\nChoice: ");
    scanf("%d", &choice);
    getchar(); // Consume newline

    char searchTerm[MAX_DESCRIPTION_LENGTH];
    printf("Enter search term: ");
    fgets(searchTerm, MAX_DESCRIPTION_LENGTH, stdin);
    searchTerm[strcspn(searchTerm, "\n")] = 0; // Remove trailing newline

    int len = strlen(searchTerm);

    // Check for empty search term
    if (len == 0) {
        printf("Please enter a search term.\n");
        return;
    }

    // Check for invalid search category
    if (choice < 1 || choice > 6) {
        printf("Invalid search category.\n");
        return;
    }

    // Check for empty claim array
    if (claimArray->count == 0) {
        printf("No claims to search.\n");
        return;
    }

    printf("Search results:\n");
	for (int i = 0; i < claimArray->count; i++) {
    int match = 0;

    switch (choice) {
        case 1: // ID
            if (claimArray->claims[i].id == atoi(searchTerm)) match = 1;
            break;
        case 2: // Username
            if (strcasecmp(claimArray->claims[i].username, searchTerm) == 0) match = 1;
            break;
        case 3: // Category
            if (strcasecmp(getCategoryString(claimArray->claims[i].category), searchTerm) == 0) match = 1;
            break;
        case 4: // Status
            if ((strcasecmp(searchTerm, "pending") == 0 && claimArray->claims[i].status == PENDING) ||
                (strcasecmp(searchTerm, "in progress") == 0 && claimArray->claims[i].status == IN_PROGRESS) ||
                (strcasecmp(searchTerm, "resolved") == 0 && claimArray->claims[i].status == RESOLVED) ||
                (strcasecmp(searchTerm, "rejected") == 0 && claimArray->claims[i].status == REJECTED)) {
                    match = 1;
            }
            break;
        case 5: // Priority
            if ((strcasecmp(searchTerm, "high") == 0 && claimArray->claims[i].priority == HIGH) ||
                (strcasecmp(searchTerm, "medium") == 0 && claimArray->claims[i].priority == MEDIUM) ||
                (strcasecmp(searchTerm, "low") == 0 && claimArray->claims[i].priority == LOW)) {
                    match = 1;
            }
            break;
        case 6: // Submission Date
            formatDate(claimArray->claims[i].submission_date, dateStr);
            if (strstr(dateStr, searchTerm) != NULL) match = 1;
            break;
    }

    if (match && (role == ADMIN || role == CLAIM_AGENT || 
        strcasecmp(claimArray->claims[i].username, searchTerm) == 0)) {
        char dateStr[DATE_STRING_LENGTH];
        formatDate(claimArray->claims[i].submission_date, dateStr);
        printf("ID: %d, Username: %s, Category: %s, Status: %s, Priority: %s, Submission Date: %s\n",
        claimArray->claims[i].id, claimArray->claims[i].username, getCategoryString(claimArray->claims[i].category),
        getStatusString(claimArray->claims[i].status), getPriorityString(claimArray->claims[i].priority), dateStr);
    }
}
}

void generateStatistics(struct ClaimArray* claimArray, char* filename) {
    // Open the file in write mode
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        printf("Error opening file for writing.\n");
        return;
    }

    // Initialize variables
    int totalClaims = claimArray->count;

    int priorityCounts[4] = {0}; // Initialize priority counts to 0
    int statusCounts[3] = {0}; // Initialize status counts to 0
    int categoryCounts[5] = {0}; // Initialize category counts to 0

    double priorityProcessingTimes[4] = {0}; // Initialize priority processing times to 0
    int priorityClaimCounts[4] = {0}; // Initialize priority claim counts to 0

    double statusProcessingTimes[3] = {0}; // Initialize status processing times to 0
    int statusClaimCounts[3] = {0}; // Initialize status claim counts to 0

    // Calculate statistics
    for (int i = 0; i < claimArray->count; i++) {
        // Update priority counts
        priorityCounts[claimArray->claims[i].priority - 1]++;

        // Update status counts
        statusCounts[claimArray->claims[i].status - 1]++;

        // Update category counts
        categoryCounts[claimArray->claims[i].category - 1]++;

        // Calculate processing time for each priority and status
        time_t submission_date = claimArray->claims[i].submission_date;
        time_t last_status_change = claimArray->claims[i].last_status_change;
        double processing_time = difftime(last_status_change, submission_date) / 60; // Convert to minutes

        priorityProcessingTimes[claimArray->claims[i].priority - 1] += processing_time;
        priorityClaimCounts[claimArray->claims[i].priority - 1]++;

        statusProcessingTimes[claimArray->claims[i].status - 1] += processing_time;
        statusClaimCounts[claimArray->claims[i].status - 1]++;
    }

    // Write statistics to file
    fprintf(file, "Total Claims: %d\n", totalClaims);

    fprintf(file, "**********************************\n");
    fprintf(file, "Priority Metrics:\n");
    fprintf(file, "**********************************\n");
    for (int i = 0; i < 4; i++) {
        double percentage = (double)priorityCounts[i] / totalClaims * 100;
        fprintf(file, "Priority %s: %d (%.2f%%)\n", getPriorityString(i + 1), priorityCounts[i], percentage);
    }
    fprintf(file, "**********************************\n");
    fprintf(file, "Status Metrics:\n");
    fprintf(file, "**********************************\n");
    for (int i = 0; i < 3; i++) {
        double percentage = (double)statusCounts[i] / totalClaims * 100;
        fprintf(file, "Status %s: %d (%.2f%%)\n", getStatusString(i + 1), statusCounts[i], percentage);
    }

    fprintf(file, "**********************************\n");
    fprintf(file, "Category Metrics:\n");
    fprintf(file, "**********************************\n");
    for (int i = 0; i < 5; i++) {
        double percentage = (double)categoryCounts[i] / totalClaims * 100;
        fprintf(file, "Category %s: %d (%.2f%%)\n", getCategoryString(i + 1), categoryCounts[i], percentage);
    }

    fprintf(file, "**********************************\n");
    fprintf(file, "Average Processing Time by Priority:\n");
    fprintf(file, "**********************************\n");
    for (int i = 0; i < 4; i++) {
        double average_processing_time = priorityProcessingTimes[i] / priorityClaimCounts[i];
        fprintf(file, "Priority %s: %.2f minutes\n", getPriorityString(i + 1), average_processing_time);
    }

    fprintf(file, "**********************************\n");
    fprintf(file, "Average Processing Time by Status:\n");
    fprintf(file, "**********************************\n");
    for (int i = 0; i < 3; i++) {
        double average_processing_time = statusProcessingTimes[i] / statusClaimCounts[i];
        fprintf(file, "Status %s: %.2f minutes\n", getStatusString(i + 1), average_processing_time);
    }

    // Close the file
    fclose(file);
}


void manageClaims(struct ClaimArray* claimArray, const char* username, enum UserRole role, int claim_id) {
    int choice;
    
    for (int i = 0; i < claimArray->count; i++) {
        if (claimArray->claims[i].id == claim_id) {
            // Check if the user is authorized to modify this claim
            if (role == CLIENT && strcmp(claimArray->claims[i].username, username) != 0) {
                printf("You are not authorized to modify this claim.\n");
                return;
            }
            
            // Check if 24 hours have passed for client modifications
            if (role == CLIENT && !isWithin24Hours(claimArray->claims[i].submission_date)) {
                printf("You can no longer modify or delete this claim as 24 hours have passed since submission.\n");
                return;
            }
            
            printf("Current status: %s\n", getStatusString(claimArray->claims[i].status));
            printf("Current priority: %s\n", getPriorityString(claimArray->claims[i].priority));
            
            if (role == CLIENT) {
                printf("1. Edit Description\n2. Delete claim\n");
            } else {
                printf("1. Edit Description\n2. Mark as In Progress\n3. Mark as Resolved\n4. Mark as Rejected\n5. Delete claim\n");
            }
            printf("Choice: ");
            scanf("%d", &choice);
            getchar(); // Consume newline
            
			char *description_lower = strdup(claimArray->claims[i].description);
            switch (choice) {
                case 1:
                    printf("Enter new description: ");
                    fgets(claimArray->claims[i].description, MAX_DESCRIPTION_LENGTH, stdin);
                    claimArray->claims[i].description[strcspn(claimArray->claims[i].description, "\n")] = 0;
                    
					for (int j = 0; description_lower[j]; j++) {
						description_lower[j] = tolower(description_lower[j]);
					}
					
					if (strstr(description_lower, "urgent") || strstr(description_lower, "emergency") || 
						strstr(description_lower, "critical") || strstr(description_lower, "severe")) {
							claimArray->claims[i].priority = HIGH;
						} else if (strstr(description_lower, "important") || strstr(description_lower, "significant") || 
							strstr(description_lower, "moderate")) {
								claimArray->claims[i].priority = MEDIUM;
							} else {
								claimArray->claims[i].priority = LOW;
							}
					
					free(description_lower);
					printf("Description updated. New priority: %s\n", 
						claimArray->claims[i].priority == HIGH ? "High" : 
						(claimArray->claims[i].priority == MEDIUM ? "Medium" : "Low"));
					break;
                case 2:
                    if (role == CLIENT) {
						for (int j = i; j < claimArray->count - 1; j++) {
                            claimArray->claims[j] = claimArray->claims[j + 1];
                        }
                        claimArray->count--;
                        printf("Claim deleted successfully.\n");
                        return;                    
					} else {
                        claimArray->claims[i].status = IN_PROGRESS;
                        claimArray->claims[i].last_status_change = time(NULL);
                        printf("Enter resolution note: ");
                        fgets(claimArray->claims[i].resolution_note, MAX_DESCRIPTION_LENGTH, stdin);
                        claimArray->claims[i].resolution_note[strcspn(claimArray->claims[i].resolution_note, "\n")] = 0;
                        printf("Claim status updated to In Progress.\n");
                    }
                    break;
                case 3:
                    if (role != CLIENT) {
                        claimArray->claims[i].status = RESOLVED;
                        claimArray->claims[i].last_status_change = time(NULL);
                        printf("Enter resolution note: ");
                        fgets(claimArray->claims[i].resolution_note, MAX_DESCRIPTION_LENGTH, stdin);
                        claimArray->claims[i].resolution_note[strcspn(claimArray->claims[i].resolution_note, "\n")] = 0;
                        printf("Claim status updated to Resolved.\n");
                    }
                    break;
                case 4:
                    if (role != CLIENT) {
                        claimArray->claims[i].status = REJECTED;
                        claimArray->claims[i].last_status_change = time(NULL);
                        printf("Enter resolution note: ");
                        fgets(claimArray->claims[i].resolution_note, MAX_DESCRIPTION_LENGTH, stdin);
                        claimArray->claims[i].resolution_note[strcspn(claimArray->claims[i].resolution_note, "\n")] = 0;
                        printf("Claim status updated to Rejected.\n");
                    }
                    break;
				case 5:
					if (role == ADMIN || (role == CLIENT && isWithin24Hours(claimArray->claims[i].submission_date))) {
						// Move all claims after this one back by one position
						for (int j = i; j < claimArray->count - 1; j++) {
							claimArray->claims[j] = claimArray->claims[j + 1];
						}
						claimArray->count--;
						printf("Claim deleted successfully.\n");
					} else {
						printf("You are not authorized to delete this claim.\n");
					}
					return;
				default:
					printf("Invalid choice.\n");
					return;
            }
            printf("Claim updated successfully.\n");
            return;
        }
    }
    printf("Claim not found.\n");
}


void manageUsers(struct UserArray* userArray) {
	char username[MAX_USERNAME_LENGTH];
	int choice;
	
	printf("Enter username to manage: ");
	fgets(username, MAX_USERNAME_LENGTH, stdin);
	username[strcspn(username, "\n")] = 0;
	
	for (int i = 0; i < userArray->count; i++) {
		if (strcmp(userArray->users[i].username, username) == 0) {
			printf("Current role: %s\n", 
				userArray->users[i].role == ADMIN ? "Admin" :
				(userArray->users[i].role == CLAIM_AGENT ? "Claim Agent" : "Client"));
			
			printf("1. Change to Admin\n2. Change to Claim Agent\n3. Change to Client\n4. Delete user\nChoice: ");
			scanf("%d", &choice);
			getchar(); // Consume newline
			
			switch (choice) {
				case 1:
					userArray->users[i].role = ADMIN;
					break;
				case 2:
					userArray->users[i].role = CLAIM_AGENT;
					break;
				case 3:
					userArray->users[i].role = CLIENT;
					break;
				case 4:
					// Move all users after this one back by one position
					for (int j = i; j < userArray->count - 1; j++) {
						userArray->users[j] = userArray->users[j + 1];
					}
					userArray->count--;
					printf("User deleted successfully.\n");
					return;
				default:
					printf("Invalid choice.\n");
					return;
			}
			printf("User role updated successfully.\n");
			return;
		}
	}
	printf("User not found.\n");
}


const char* getCategoryString(enum ClaimCategory category) {
	switch(category) {
		case SHIPPING: return "shipping";
		case RETURN: return "return";
		case PAYMENT: return "payment";
		case CUSTOMER_SERVICES: return "customer service";
		case TECHNICAL: return "technical";
		default: return "Unknown";
	}
}

const char* getStatusString(enum ClaimStatus status) {
	switch(status) {
		case PENDING: return "Pending";
		case IN_PROGRESS: return "In Progress";
		case RESOLVED: return "Resolved";
		case REJECTED: return "Rejected";
		default: return "Unknown";
	}
}

const char* getPriorityString(enum ClaimPriority priority) {
	return priority == HIGH ? "High" : (priority == MEDIUM ? "Medium" : "Low");
}


int isWithin24Hours(time_t submissionTime) {
	time_t currentTime = time(NULL);
	double diff = difftime(currentTime, submissionTime);
	return diff <= (24 * 60 * 60); // 24 hours in seconds
}

void printLine(int num) {
    for (int i=0; i<num; i++)
		printf("_");
	printf("\n");
}

void printSlashes(int num) {
    for (int i=0; i<num; i++)
		printf("/");
	printf("\n");
}

void printAsterics(int num) {
    for (int i=0; i<num; i++)
		printf("*");
	printf("\n");
}

void padding(int num) {
    for (int i=0; i<num; i++)
		printf("\n");
}

char* strcasestr_custom(const char* haystack, const char* needle) {
    if (!*needle) {
        return (char*)haystack;
    }
    
    for (; *haystack; haystack++) {
        if (tolower((unsigned char)*haystack) == tolower((unsigned char)*needle)) {
            const char* h = haystack;
            const char* n = needle;
            while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
                h++;
                n++;
            }
            if (!*n) {
                return (char*)haystack;
            }
        }
    }
    return NULL;
}


void printTableHeader() {
    printf("+-----+--------------------------+-----------------+---------------+--------------+-----------------+\n");
    printf("| ID  | Category                 | Status          | Priority      | Username     | Date            |\n");
    printf("+-----+--------------------------+-----------------+---------------+--------------+-----------------+\n");
}

void printTableRow(int id, const char* category, const char* status, const char* priority, const char* username, const char* date) {
    printf("| %-7d | %-14s | %-15s | %-13s | %-12s | %-10s |\n", 
           id, category, status, priority, username, date);
}

void printTableFooter() {
    printf("+---------+----------------+-----------------+---------------+--------------+------------+\n");
}

void generateDailyReport(struct ClaimArray* claimArray, const char* filename) {
    time_t currentTime = time(NULL);
    struct tm* currentTm = localtime(&currentTime);
    int currentDay = currentTm->tm_mday;
    int currentMonth = currentTm->tm_mon + 1; // Months are 0-based in C
    int currentYear = currentTm->tm_year + 1900; // Years are since 1900 in C

    FILE* reportFile = fopen(filename, "w");
    if (reportFile == NULL) {
        printf("Error opening report file for writing.\n");
        return;
    }

    fprintf(reportFile, "Daily Report for %d-%d-%d\n\n", currentYear, currentMonth, currentDay);

    fprintf(reportFile, "New Complaints:\n");
    fprintf(reportFile, "_________________\n");
    for (int i = 0; i < claimArray->count; i++) {
        struct tm* submissionTm = localtime(&claimArray->claims[i].submission_date);
        char submission_date_str[20];
        strftime(submission_date_str, 20, "%Y-%m-%d %H:%M:%S", submissionTm);
        if (submissionTm->tm_mday == currentDay && submissionTm->tm_mon == currentMonth - 1 && submissionTm->tm_year == currentYear - 1900) {
            fprintf(reportFile, "ID: %d, Username: %s, Category: %s, Priority: %s [%s]\n",
                    claimArray->claims[i].id,
                    claimArray->claims[i].username,
                    getCategoryString(claimArray->claims[i].category),
                    getPriorityString(claimArray->claims[i].priority),
                    submission_date_str);
        }
    }

    fprintf(reportFile, "\nResolved Complaints:\n");
    fprintf(reportFile, "________________________\n");
    for (int i = 0; i < claimArray->count; i++) {
        struct tm* lastStatusChangeTm = localtime(&claimArray->claims[i].last_status_change);
        char last_status_change_str[20];
        strftime(last_status_change_str, 20, "%Y-%m-%d %H:%M:%S", lastStatusChangeTm);
        if (claimArray->claims[i].status == RESOLVED && lastStatusChangeTm->tm_mday == currentDay && lastStatusChangeTm->tm_mon == currentMonth - 1 && lastStatusChangeTm->tm_year == currentYear - 1900) {
            fprintf(reportFile, "ID: %d, Username: %s, Category: %s, Priority: %s, Last Status Change: %s\n",
                    claimArray->claims[i].id,
                    claimArray->claims[i].username,
                    getCategoryString(claimArray->claims[i].category),
                    getPriorityString(claimArray->claims[i].priority),
                    last_status_change_str);
        }
    }

    fclose(reportFile);
    printf("Daily report generated successfully.\n");
}
