int main() {
    sqlite3 *db;
    sqlite3_stmt *res;
    
    // 1. Open the database file
    if (sqlite3_open("users.db", &db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    // 2. Prepare the SQL query
    const char *sql = "SELECT id, name, score FROM users";
    if (sqlite3_prepare_v2(db, sql, -1, &res, 0) != SQLITE_OK) {
        fprintf(stderr, "Failed to fetch data: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    // 3. Iterate through rows and populate structs
    // (In a real app, you'd count rows or realloc dynamically)
    User users[3]; 
    int i = 0;

    while (sqlite3_step(res) == SQLITE_ROW && i < 3) {
        users[i].id = sqlite3_column_int(res, 0);
        
        // Handle string copying safely
        const unsigned char *name = sqlite3_column_text(res, 1);
        strncpy(users[i].name, (char*)name, 49);
        
        users[i].score = sqlite3_column_double(res, 2);
        
        printf("Loaded: %d | %s | %.1f\n", users[i].id, users[i].name, users[i].score);
        i++;
    }

    // 4. Cleanup
    sqlite3_finalize(res);
    sqlite3_close(db);

    return 0;
}