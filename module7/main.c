/* main.c - menu for the cloud billing system */

/*
 * Settings (environment variables, all optional):
 *   BILLING_DATA_DIR       folder for the data (default "data")
 *   BILLING_PORT           TCP port for usage ingest (default 5050)
 *   BILLING_LOG_MAX_BYTES  log size before it rotates (default 1048576)
 * Exit codes: 0 ok, 2 bad settings, 3 database cannot be opened
 */

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include "billing.h"

/* Print a time as YYYY-MM-DD. */
static void format_date(time_t t, char *buf, int size)
{
    strftime(buf, size, "%Y-%m-%d", localtime(&t));
}

static void print_usage_header(void)
{
    printf("\n%-5s %-5s %-8s %-11s %10s %10s %10s %10s %10s\n",
           "ID", "Acct", "Type", "Date", "CPU-hrs", "Mem-GBhrs", "Storage-GB", "In-MB", "Out-MB");
}

static void print_usage_row(UsageRecord *u)
{
    char date[16];

    format_date(u->timestamp, date, sizeof(date));
    printf("%-5d %-5d %-8s %-11s %10.2f %10.2f %10.2f %10.2f %10.2f\n",
           u->id, u->account_id, resource_name(u->resource), date,
           u->cpu_hours, u->mem_gb_hours, u->storage_gb, u->net_in_mb, u->net_out_mb);
}

/* ---------- accounts ---------- */

static void create_account(void)
{
    Account a;

    memset(&a, 0, sizeof(a));

    if (read_line("Account name: ", a.name, NAME_LEN) != 0) return;
    if (a.name[0] == '\0') {
        printf("Name cannot be empty.\n");
        return;
    }
    if (read_line("Email: ", a.email, EMAIL_LEN) != 0) return;
    a.created = time(NULL);

    if (db_account_create(&a) != 0) {
        printf("Could not create the account.\n");
        return;
    }
    printf("Account created (ID: %d).\n", a.id);
    log_event("account created id=%d name=%s", a.id, a.name);
}

static void view_accounts(void)
{
    Account *list = NULL;
    char date[16];
    int count;
    int i;

    count = db_account_list(&list);
    if (count < 0) {
        printf("Could not read the accounts.\n");
        return;
    }
    if (count == 0) {
        printf("No accounts.\n");
        free(list);
        return;
    }

    printf("\n%-5s %-24s %-28s %-11s\n", "ID", "Name", "Email", "Created");
    for (i = 0; i < count; i++) {
        format_date(list[i].created, date, sizeof(date));
        printf("%-5d %-24.24s %-28.28s %-11s\n", list[i].id, list[i].name, list[i].email, date);
    }
    free(list);
}

static void update_account(void)
{
    Account a;
    char text[NAME_LEN];
    int id;
    int result;

    if (read_int("Account ID: ", &id) != 0) return;

    result = db_account_get(id, &a);
    if (result == 1) {
        printf("Account %d not found.\n", id);
        return;
    }
    if (result < 0) {
        printf("Database error.\n");
        return;
    }

    printf("Current name: %s\n", a.name);
    if (read_line("New name (Enter to keep): ", text, NAME_LEN) != 0) return;
    if (text[0] != '\0') {
        strcpy(a.name, text);
    }

    printf("Current email: %s\n", a.email);
    if (read_line("New email (Enter to keep): ", text, EMAIL_LEN) != 0) return;
    if (text[0] != '\0') {
        strcpy(a.email, text);
    }

    if (db_account_update(&a) != 0) {
        printf("Could not update the account.\n");
        return;
    }
    printf("Account updated.\n");
    log_event("account updated id=%d", a.id);
}

static void delete_account(void)
{
    int id;
    int result;
    int removed;

    if (read_int("Account ID: ", &id) != 0) return;

    result = db_account_delete(id);
    if (result == 1) {
        printf("Account %d not found.\n", id);
        return;
    }
    if (result < 0) {
        printf("Database error.\n");
        return;
    }

    /* the account's usage records are deleted too */
    removed = db_usage_delete_for_account(id);
    if (removed < 0) removed = 0;
    printf("Account deleted (%d usage record(s) removed).\n", removed);
    log_event("account deleted id=%d usage_removed=%d", id, removed);
}

/* ---------- usage records ---------- */

static void add_usage(void)
{
    Account a;
    UsageRecord u;
    char date_text[32];
    double v1 = 0;
    double v2 = 0;
    int account_id;
    int resource;
    int result;
    time_t when;

    if (read_int("Account ID: ", &account_id) != 0) return;

    result = db_account_get(account_id, &a);
    if (result == 1) {
        printf("Account %d not found.\n", account_id);
        return;
    }
    if (result < 0) {
        printf("Database error.\n");
        return;
    }

    if (read_int("Resource (1=Server, 2=Storage, 3=Network): ", &resource) != 0) return;

    if (resource == RES_SERVER) {
        if (read_double("CPU usage (vCPU-hours): ", &v1) != 0) return;
        if (read_double("Memory usage (GB-hours): ", &v2) != 0) return;
    } else if (resource == RES_STORAGE) {
        if (read_double("Storage (GB, fractions are rounded down): ", &v1) != 0) return;
    } else if (resource == RES_NETWORK) {
        if (read_double("Inbound data (MB, free): ", &v1) != 0) return;
        if (read_double("Outbound data (MB): ", &v2) != 0) return;
    } else {
        printf("Invalid resource.\n");
        return;
    }

    usage_fill(&u, resource, v1, v2);
    u.account_id = account_id;
    u.timestamp = time(NULL);

    if (read_line("Date YYYY-MM-DD (Enter for today): ", date_text, sizeof(date_text)) != 0) return;
    if (date_text[0] != '\0') {
        when = parse_date(date_text);
        if (when == (time_t)-1) {
            printf("Invalid date. Nothing saved.\n");
            return;
        }
        u.timestamp = when;
    }

    if (db_usage_create(&u) != 0) {
        printf("Could not save the usage.\n");
        return;
    }
    printf("Usage record created (ID: %d).\n", u.id);
    log_event("usage created id=%d account=%d type=%s", u.id, account_id, resource_name(resource));
}

static void view_usage(void)
{
    UsageRecord *list = NULL;
    int account_id;
    int count;
    int i;

    if (read_int("Account ID (0 for all accounts): ", &account_id) != 0) return;

    count = db_usage_list(account_id, &list);
    if (count < 0) {
        printf("Could not read the usage.\n");
        return;
    }
    if (count == 0) {
        printf("No usage records.\n");
        free(list);
        return;
    }

    print_usage_header();
    for (i = 0; i < count; i++) {
        print_usage_row(&list[i]);
    }
    printf("\n%d record(s).\n", count);
    free(list);
}

static void update_usage(void)
{
    UsageRecord u;
    int id;
    int result;

    if (read_int("Usage record ID: ", &id) != 0) return;

    result = db_usage_get(id, &u);
    if (result == 1) {
        printf("Usage record %d not found.\n", id);
        return;
    }
    if (result < 0) {
        printf("Database error.\n");
        return;
    }

    print_usage_header();
    print_usage_row(&u);
    printf("Press Enter to keep a value.\n");

    if (u.resource == RES_SERVER) {
        if (read_double_keep("New CPU hours: ", &u.cpu_hours) != 0) return;
        if (read_double_keep("New memory GB-hours: ", &u.mem_gb_hours) != 0) return;
    } else if (u.resource == RES_STORAGE) {
        if (read_double_keep("New storage GB: ", &u.storage_gb) != 0) return;
    } else if (u.resource == RES_NETWORK) {
        if (read_double_keep("New inbound MB: ", &u.net_in_mb) != 0) return;
        if (read_double_keep("New outbound MB: ", &u.net_out_mb) != 0) return;
    }

    if (db_usage_update(&u) != 0) {
        printf("Could not update the usage.\n");
        return;
    }
    printf("Usage record updated.\n");
    log_event("usage updated id=%d", u.id);
}

static void delete_usage(void)
{
    int id;
    int result;

    if (read_int("Usage record ID: ", &id) != 0) return;

    result = db_usage_delete(id);
    if (result == 1) {
        printf("Usage record %d not found.\n", id);
        return;
    }
    if (result < 0) {
        printf("Database error.\n");
        return;
    }
    printf("Usage record deleted.\n");
    log_event("usage deleted id=%d", id);
}

/* ---------- ingest, invoices, summary, export ---------- */

static void ingest_usage(void)
{
    printf("In another terminal run: ./meter_agent %d <account_id> <count>\n", cfg.port);
    if (ingest_run(cfg.port) != 0) {
        printf("Ingest failed.\n");
    }
}

/* Ask for a billing period until it is valid. Returns -1 at end of input. */
static int ask_period(char *buf, int size)
{
    while (read_line("Billing period YYYY-MM (Enter for all time): ", buf, size) == 0) {
        if (valid_period(buf)) return 0;
        printf("Use the format YYYY-MM, for example 2026-10.\n");
    }
    return -1;
}

static void generate_invoice(void)
{
    Account a;
    Invoice inv;
    char period[16];
    char path[PATH_LEN];
    int id;
    int result;

    if (read_int("Account ID: ", &id) != 0) return;

    result = db_account_get(id, &a);
    if (result == 1) {
        printf("Account %d not found.\n", id);
        return;
    }
    if (result < 0) {
        printf("Database error.\n");
        return;
    }

    if (ask_period(period, sizeof(period)) != 0) return;

    if (invoice_build(&inv, &a, period) != 0) {
        printf("Could not build the invoice.\n");
        return;
    }

    printf("\n");
    invoice_print(&inv, stdout);

    if (invoice_write(&inv, path, PATH_LEN) == 0) {
        printf("Invoice saved to %s\n", path);
        log_event("invoice written account=%d total=%.2f file=%s", id, inv.total, path);
    } else {
        printf("Could not write the invoice file.\n");
    }
}

static void billing_summary(void)
{
    char period[16];

    if (ask_period(period, sizeof(period)) != 0) return;
    if (summary_print(period) != 0) {
        printf("Could not build the summary.\n");
    }
}

static void export_data(void)
{
    char period[16];
    char path[PATH_LEN];
    int format;

    if (ask_period(period, sizeof(period)) != 0) return;
    if (read_int("Format (1=CSV, 2=JSON): ", &format) != 0) return;
    if (format != 1 && format != 2) {
        printf("Invalid format.\n");
        return;
    }

    if (export_summary(period, format == 2, path, PATH_LEN) != 0) {
        printf("Export failed.\n");
        return;
    }
    printf("Exported to %s\n", path);
    log_event("export written file=%s", path);
}

/* ---------- main ---------- */

static void print_menu(void)
{
    printf("\n=== Cloud Services Billing System ===\n");
    printf(" 1. Create account\n");
    printf(" 2. View accounts\n");
    printf(" 3. Update account\n");
    printf(" 4. Delete account\n");
    printf(" 5. Add usage record\n");
    printf(" 6. View usage records\n");
    printf(" 7. Update usage record\n");
    printf(" 8. Delete usage record\n");
    printf(" 9. Receive usage from metering agent (TCP)\n");
    printf("10. Generate invoice\n");
    printf("11. Billing summary\n");
    printf("12. Export summary (CSV/JSON)\n");
    printf(" 0. Exit\n");
}

int main(void)
{
    int choice;

    /* do not crash if the agent disconnects */
    signal(SIGPIPE, SIG_IGN);

    if (config_load() != 0) return EXIT_CONFIG_ERROR;
    limits_apply();
    if (db_check() != 0) {
        fprintf(stderr, "Could not open the databases in '%s'.\n", cfg.data_dir);
        return EXIT_DB_ERROR;
    }
    log_event("billing system started");

    while (1) {
        print_menu();
        if (read_int("Choice: ", &choice) != 0) break;

        if (choice == 1) create_account();
        else if (choice == 2) view_accounts();
        else if (choice == 3) update_account();
        else if (choice == 4) delete_account();
        else if (choice == 5) add_usage();
        else if (choice == 6) view_usage();
        else if (choice == 7) update_usage();
        else if (choice == 8) delete_usage();
        else if (choice == 9) ingest_usage();
        else if (choice == 10) generate_invoice();
        else if (choice == 11) billing_summary();
        else if (choice == 12) export_data();
        else if (choice == 0) {
            printf("Goodbye!\n");
            break;
        } else {
            printf("Invalid choice.\n");
        }
    }

    log_event("billing system exited");
    return EXIT_OK;
}
