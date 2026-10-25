/* db.c - gdbm database for accounts and usage records */

/*
 * Two database files are used: accounts.db and usage.db.
 * Each record is saved as a struct. The key is the id as text.
 * The key "next_id" holds the next id to give out.
 * The database is opened and closed in every function.
 */

#include <gdbm.h>
#include <stdlib.h>
#include <string.h>
#include "billing.h"

#define ACCOUNTS_DB "accounts.db"
#define USAGE_DB "usage.db"
#define NEXT_ID_KEY "next_id"

/* Open a database file in the data folder. Make it if it does not exist. */
static GDBM_FILE open_db(char *name)
{
    char path[PATH_LEN];
    GDBM_FILE db;

    path_in_data(path, PATH_LEN, name);
    db = gdbm_open(path, 0, GDBM_WRCREAT, 0644, NULL);
    if (db == NULL) {
        fprintf(stderr, "Database error (%s): %s\n", name, gdbm_strerror(gdbm_errno));
    }
    return db;
}

/* Make the key for an id. */
static datum make_key(char *text, int id)
{
    datum key;

    sprintf(text, "%d", id);
    key.dptr = text;
    key.dsize = strlen(text) + 1;
    return key;
}

/* Get the next id and save the counter. Returns 0 if it fails. */
static int get_next_id(GDBM_FILE db)
{
    datum key;
    datum value;
    datum new_value;
    int id = 1;
    int next;

    key.dptr = NEXT_ID_KEY;
    key.dsize = strlen(NEXT_ID_KEY) + 1;

    value = gdbm_fetch(db, key);
    if (value.dptr != NULL) {
        memcpy(&id, value.dptr, sizeof(int));
        free(value.dptr);
    }

    next = id + 1;
    new_value.dptr = (char *)&next;
    new_value.dsize = sizeof(int);
    if (gdbm_store(db, key, new_value, GDBM_REPLACE) != 0) return 0;
    return id;
}

/* Check that both databases can be opened. */
int db_check(void)
{
    GDBM_FILE db;

    db = open_db(ACCOUNTS_DB);
    if (db == NULL) return -1;
    gdbm_close(db);

    db = open_db(USAGE_DB);
    if (db == NULL) return -1;
    gdbm_close(db);
    return 0;
}

/* ---------- accounts ---------- */

/* Create: save a new account and set its id. */
int db_account_create(Account *a)
{
    GDBM_FILE db;
    datum key;
    datum value;
    char text[16];
    int result;

    db = open_db(ACCOUNTS_DB);
    if (db == NULL) return -1;

    a->id = get_next_id(db);
    if (a->id == 0) {
        gdbm_close(db);
        return -1;
    }

    key = make_key(text, a->id);
    value.dptr = (char *)a;
    value.dsize = sizeof(Account);
    result = gdbm_store(db, key, value, GDBM_INSERT);

    gdbm_close(db);
    if (result != 0) return -1;
    return 0;
}

/* Read: get one account by id. */
int db_account_get(int id, Account *out)
{
    GDBM_FILE db;
    datum key;
    datum value;
    char text[16];

    db = open_db(ACCOUNTS_DB);
    if (db == NULL) return -1;

    key = make_key(text, id);
    value = gdbm_fetch(db, key);
    gdbm_close(db);

    if (value.dptr == NULL) return 1;

    memcpy(out, value.dptr, sizeof(Account));
    free(value.dptr);
    return 0;
}

/* Update: replace an account that already exists. */
int db_account_update(Account *a)
{
    GDBM_FILE db;
    datum key;
    datum value;
    char text[16];
    int result;

    db = open_db(ACCOUNTS_DB);
    if (db == NULL) return -1;

    key = make_key(text, a->id);
    if (!gdbm_exists(db, key)) {
        gdbm_close(db);
        return 1;
    }

    value.dptr = (char *)a;
    value.dsize = sizeof(Account);
    result = gdbm_store(db, key, value, GDBM_REPLACE);

    gdbm_close(db);
    if (result != 0) return -1;
    return 0;
}

/* Delete: remove an account. */
int db_account_delete(int id)
{
    GDBM_FILE db;
    datum key;
    char text[16];
    int result;

    db = open_db(ACCOUNTS_DB);
    if (db == NULL) return -1;

    key = make_key(text, id);
    if (!gdbm_exists(db, key)) {
        gdbm_close(db);
        return 1;
    }

    result = gdbm_delete(db, key);
    gdbm_close(db);
    if (result != 0) return -1;
    return 0;
}

/* List: get all accounts sorted by id. Returns the count. Caller frees. */
int db_account_list(Account **out)
{
    GDBM_FILE db;
    datum key;
    datum next_key;
    datum value;
    Account *list = NULL;
    Account *bigger;
    Account temp;
    int count = 0;
    int i, j;

    db = open_db(ACCOUNTS_DB);
    if (db == NULL) return -1;

    key = gdbm_firstkey(db);
    while (key.dptr != NULL) {
        next_key = gdbm_nextkey(db, key);

        if (strcmp(key.dptr, NEXT_ID_KEY) != 0) {
            value = gdbm_fetch(db, key);
            if (value.dptr != NULL) {
                bigger = realloc(list, (count + 1) * sizeof(Account));
                if (bigger == NULL) {
                    free(value.dptr);
                    free(key.dptr);
                    free(next_key.dptr);
                    free(list);
                    gdbm_close(db);
                    return -1;
                }
                list = bigger;
                memcpy(&list[count], value.dptr, sizeof(Account));
                count++;
                free(value.dptr);
            }
        }

        free(key.dptr);
        key = next_key;
    }
    gdbm_close(db);

    /* sort by id (bubble sort) */
    for (i = 0; i < count - 1; i++) {
        for (j = 0; j < count - 1 - i; j++) {
            if (list[j].id > list[j + 1].id) {
                temp = list[j];
                list[j] = list[j + 1];
                list[j + 1] = temp;
            }
        }
    }

    *out = list;
    return count;
}

/* ---------- usage records ---------- */

/* Create: save a new usage record and set its id. */
int db_usage_create(UsageRecord *u)
{
    GDBM_FILE db;
    datum key;
    datum value;
    char text[16];
    int result;

    db = open_db(USAGE_DB);
    if (db == NULL) return -1;

    u->id = get_next_id(db);
    if (u->id == 0) {
        gdbm_close(db);
        return -1;
    }

    key = make_key(text, u->id);
    value.dptr = (char *)u;
    value.dsize = sizeof(UsageRecord);
    result = gdbm_store(db, key, value, GDBM_INSERT);

    gdbm_close(db);
    if (result != 0) return -1;
    return 0;
}

/* Read: get one usage record by id. */
int db_usage_get(int id, UsageRecord *out)
{
    GDBM_FILE db;
    datum key;
    datum value;
    char text[16];

    db = open_db(USAGE_DB);
    if (db == NULL) return -1;

    key = make_key(text, id);
    value = gdbm_fetch(db, key);
    gdbm_close(db);

    if (value.dptr == NULL) return 1;

    memcpy(out, value.dptr, sizeof(UsageRecord));
    free(value.dptr);
    return 0;
}

/* Update: replace a usage record that already exists. */
int db_usage_update(UsageRecord *u)
{
    GDBM_FILE db;
    datum key;
    datum value;
    char text[16];
    int result;

    db = open_db(USAGE_DB);
    if (db == NULL) return -1;

    key = make_key(text, u->id);
    if (!gdbm_exists(db, key)) {
        gdbm_close(db);
        return 1;
    }

    value.dptr = (char *)u;
    value.dsize = sizeof(UsageRecord);
    result = gdbm_store(db, key, value, GDBM_REPLACE);

    gdbm_close(db);
    if (result != 0) return -1;
    return 0;
}

/* Delete: remove a usage record. */
int db_usage_delete(int id)
{
    GDBM_FILE db;
    datum key;
    char text[16];
    int result;

    db = open_db(USAGE_DB);
    if (db == NULL) return -1;

    key = make_key(text, id);
    if (!gdbm_exists(db, key)) {
        gdbm_close(db);
        return 1;
    }

    result = gdbm_delete(db, key);
    gdbm_close(db);
    if (result != 0) return -1;
    return 0;
}

/* List: get usage records sorted by id. account_id 0 means all accounts. */
int db_usage_list(int account_id, UsageRecord **out)
{
    GDBM_FILE db;
    datum key;
    datum next_key;
    datum value;
    UsageRecord *list = NULL;
    UsageRecord *bigger;
    UsageRecord record;
    UsageRecord temp;
    int count = 0;
    int i, j;

    db = open_db(USAGE_DB);
    if (db == NULL) return -1;

    key = gdbm_firstkey(db);
    while (key.dptr != NULL) {
        next_key = gdbm_nextkey(db, key);

        if (strcmp(key.dptr, NEXT_ID_KEY) != 0) {
            value = gdbm_fetch(db, key);
            if (value.dptr != NULL) {
                memcpy(&record, value.dptr, sizeof(UsageRecord));
                free(value.dptr);

                if (account_id == 0 || record.account_id == account_id) {
                    bigger = realloc(list, (count + 1) * sizeof(UsageRecord));
                    if (bigger == NULL) {
                        free(key.dptr);
                        free(next_key.dptr);
                        free(list);
                        gdbm_close(db);
                        return -1;
                    }
                    list = bigger;
                    list[count] = record;
                    count++;
                }
            }
        }

        free(key.dptr);
        key = next_key;
    }
    gdbm_close(db);

    /* sort by id (bubble sort) */
    for (i = 0; i < count - 1; i++) {
        for (j = 0; j < count - 1 - i; j++) {
            if (list[j].id > list[j + 1].id) {
                temp = list[j];
                list[j] = list[j + 1];
                list[j + 1] = temp;
            }
        }
    }

    *out = list;
    return count;
}

/* Delete all usage records of one account. Returns how many were deleted. */
int db_usage_delete_for_account(int account_id)
{
    UsageRecord *list = NULL;
    int count;
    int deleted = 0;
    int i;

    count = db_usage_list(account_id, &list);
    if (count < 0) return -1;

    for (i = 0; i < count; i++) {
        if (db_usage_delete(list[i].id) == 0) {
            deleted++;
        }
    }

    free(list);
    return deleted;
}
