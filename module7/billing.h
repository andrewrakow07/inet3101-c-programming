/* billing.h - shared definitions for the billing system */

#ifndef BILLING_H
#define BILLING_H

#include <stdio.h>
#include <time.h>

#define NAME_LEN 64
#define EMAIL_LEN 64
#define PATH_LEN 512
#define MAX_VALUE 1000000000.0
#define LOG_KEEP 3

/* exit codes */
#define EXIT_OK 0
#define EXIT_CONFIG_ERROR 2
#define EXIT_DB_ERROR 3

/* resource types */
#define RES_SERVER 1
#define RES_STORAGE 2
#define RES_NETWORK 3

/* meters, one line each on an invoice */
#define METER_CPU 0
#define METER_MEM 1
#define METER_STORAGE 2
#define METER_NET_OUT 3
#define METER_NET_IN 4
#define METER_COUNT 5

typedef struct {
    int id;
    char name[NAME_LEN];
    char email[EMAIL_LEN];
    time_t created;
} Account;

/* Only the fields that match the resource type are used. */
typedef struct {
    int id;
    int account_id;
    int resource;
    time_t timestamp;
    double cpu_hours;
    double mem_gb_hours;
    double storage_gb;
    double net_in_mb;
    double net_out_mb;
} UsageRecord;

/* Price list for one meter. A tier applies up to its limit. */
typedef struct {
    char *name;
    char *unit;
    double limit[3];
    double rate[3];
} Meter;

/* Usage ledger: linked list of usage records */
typedef struct LedgerNode {
    UsageRecord rec;
    struct LedgerNode *next;
} LedgerNode;

typedef struct {
    LedgerNode *head;
    LedgerNode *tail;
    int count;
} Ledger;

void ledger_init(Ledger *l);
int ledger_add(Ledger *l, UsageRecord *r);
void ledger_totals(Ledger *l, double qty[METER_COUNT]);
void ledger_free(Ledger *l);

/* Invoice */
typedef struct {
    Account account;
    char period[8];
    int record_count;
    double quantity[METER_COUNT];
    double charge[METER_COUNT];
    double total;
} Invoice;

int invoice_build(Invoice *inv, Account *a, char *period);
void invoice_print(Invoice *inv, FILE *out);
int invoice_write(Invoice *inv, char *path_out, int size);

/* billing.c */
Meter *meter_get(int index);
double meter_charge(Meter *m, double quantity);
int resource_from_name(char *name);
char *resource_name(int resource);
void usage_fill(UsageRecord *u, int resource, double v1, double v2);
int summary_print(char *period);
int export_summary(char *period, int as_json, char *path_out, int size);

/* util.c */
typedef struct {
    char data_dir[PATH_LEN];
    int port;
    long log_max_bytes;
} Config;

extern Config cfg;

int config_load(void);
void path_in_data(char *out, int size, char *name);
void limits_apply(void);
void log_event(char *fmt, ...);
int read_line(char *prompt, char *buf, int size);
int read_int(char *prompt, int *out);
int read_double(char *prompt, double *out);
int read_double_keep(char *prompt, double *value);
time_t parse_date(char *s);
int valid_period(char *s);

/* db.c - return 0 on success, 1 if not found, -1 on error */
int db_check(void);

int db_account_create(Account *a);
int db_account_get(int id, Account *out);
int db_account_update(Account *a);
int db_account_delete(int id);
int db_account_list(Account **out);

int db_usage_create(UsageRecord *u);
int db_usage_get(int id, UsageRecord *out);
int db_usage_update(UsageRecord *u);
int db_usage_delete(int id);
int db_usage_list(int account_id, UsageRecord **out);
int db_usage_delete_for_account(int account_id);

/* server.c */
int ingest_run(int port);

#endif
