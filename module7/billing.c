/* billing.c - prices, ledger, invoices, summary and export */

/*
 * Billing rules:
 * - Servers are billed on CPU and memory.
 * - Storage is billed in whole GB. Fractions are rounded down.
 * - Network is billed in MB. Inbound is free.
 * The rates are sample rates.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "billing.h"

/* Price list. Each meter has 3 tiers, for example CPU is
   0.050 per hour up to 1000, 0.040 up to 5000, then 0.030. */
static Meter meters[METER_COUNT] = {
    {"Server CPU", "vCPU-hours", {1000, 5000, 1e300}, {0.050, 0.040, 0.030}},
    {"Server Memory", "GB-hours", {5000, 20000, 1e300}, {0.010, 0.008, 0.006}},
    {"Storage", "GB", {1000, 10000, 1e300}, {0.020, 0.015, 0.010}},
    {"Network Outbound", "MB", {10000, 100000, 1e300}, {0.0010, 0.0008, 0.0006}},
    {"Network Inbound", "MB", {1e300, 1e300, 1e300}, {0.0, 0.0, 0.0}}
};

Meter *meter_get(int index)
{
    return &meters[index];
}

/* Work out the charge for a quantity using the tiers. */
double meter_charge(Meter *m, double quantity)
{
    double charge = 0;
    double previous = 0;
    double in_tier;
    int i;

    for (i = 0; i < 3; i++) {
        if (quantity <= previous) break;

        if (quantity < m->limit[i]) {
            in_tier = quantity - previous;
        } else {
            in_tier = m->limit[i] - previous;
        }
        charge = charge + in_tier * m->rate[i];
        previous = m->limit[i];
    }
    return charge;
}

/* Round to cents. */
static double round_cents(double x)
{
    return floor(x * 100 + 0.5) / 100;
}

/* Turn a resource name into its number. Returns 0 if unknown. */
int resource_from_name(char *name)
{
    if (strcmp(name, "SERVER") == 0) return RES_SERVER;
    if (strcmp(name, "STORAGE") == 0) return RES_STORAGE;
    if (strcmp(name, "NETWORK") == 0) return RES_NETWORK;
    return 0;
}

char *resource_name(int resource)
{
    if (resource == RES_SERVER) return "SERVER";
    if (resource == RES_STORAGE) return "STORAGE";
    if (resource == RES_NETWORK) return "NETWORK";
    return "UNKNOWN";
}

/* Fill in a usage record.
   Server: v1 = cpu hours, v2 = memory GB-hours
   Storage: v1 = GB
   Network: v1 = inbound MB, v2 = outbound MB */
void usage_fill(UsageRecord *u, int resource, double v1, double v2)
{
    memset(u, 0, sizeof(UsageRecord));
    u->resource = resource;

    if (resource == RES_SERVER) {
        u->cpu_hours = v1;
        u->mem_gb_hours = v2;
    } else if (resource == RES_STORAGE) {
        u->storage_gb = v1;
    } else if (resource == RES_NETWORK) {
        u->net_in_mb = v1;
        u->net_out_mb = v2;
    }
}

/* ---------- ledger ---------- */

void ledger_init(Ledger *l)
{
    l->head = NULL;
    l->tail = NULL;
    l->count = 0;
}

/* Add a copy of a record to the end of the list. Returns -1 if out of memory. */
int ledger_add(Ledger *l, UsageRecord *r)
{
    LedgerNode *node = malloc(sizeof(LedgerNode));
    if (node == NULL) return -1;

    node->rec = *r;
    node->next = NULL;

    if (l->tail == NULL) {
        l->head = node;
    } else {
        l->tail->next = node;
    }
    l->tail = node;
    l->count++;
    return 0;
}

/* Add up the quantities for each meter. Storage is rounded down per record. */
void ledger_totals(Ledger *l, double qty[METER_COUNT])
{
    LedgerNode *node;
    int i;

    for (i = 0; i < METER_COUNT; i++) {
        qty[i] = 0;
    }

    for (node = l->head; node != NULL; node = node->next) {
        qty[METER_CPU] += node->rec.cpu_hours;
        qty[METER_MEM] += node->rec.mem_gb_hours;
        qty[METER_STORAGE] += floor(node->rec.storage_gb);
        qty[METER_NET_OUT] += node->rec.net_out_mb;
        qty[METER_NET_IN] += node->rec.net_in_mb;
    }
}

void ledger_free(Ledger *l)
{
    LedgerNode *node = l->head;
    LedgerNode *next;

    while (node != NULL) {
        next = node->next;
        free(node);
        node = next;
    }
    l->head = NULL;
    l->tail = NULL;
    l->count = 0;
}

/* ---------- invoice ---------- */

/* Check if a time is in the period (YYYY-MM). Empty period matches everything. */
static int in_period(time_t t, char *period)
{
    char text[16];

    if (period[0] == '\0') return 1;

    strftime(text, sizeof(text), "%Y-%m", localtime(&t));
    if (strcmp(text, period) == 0) return 1;
    return 0;
}

/* Build an invoice for one account. Returns 0 on success, -1 on error. */
int invoice_build(Invoice *inv, Account *a, char *period)
{
    UsageRecord *list = NULL;
    Ledger ledger;
    double qty[METER_COUNT];
    int count;
    int i;

    count = db_usage_list(a->id, &list);
    if (count < 0) return -1;

    ledger_init(&ledger);
    for (i = 0; i < count; i++) {
        if (in_period(list[i].timestamp, period)) {
            if (ledger_add(&ledger, &list[i]) != 0) {
                ledger_free(&ledger);
                free(list);
                return -1;
            }
        }
    }
    free(list);

    ledger_totals(&ledger, qty);

    memset(inv, 0, sizeof(Invoice));
    inv->account = *a;
    snprintf(inv->period, sizeof(inv->period), "%s", period);
    inv->record_count = ledger.count;

    for (i = 0; i < METER_COUNT; i++) {
        inv->quantity[i] = qty[i];
        inv->charge[i] = round_cents(meter_charge(&meters[i], qty[i]));
        inv->total = inv->total + inv->charge[i];
    }

    ledger_free(&ledger);
    return 0;
}

/* Print an invoice to the screen or to a file. */
void invoice_print(Invoice *inv, FILE *out)
{
    int i;

    fprintf(out, "==================================================================\n");
    fprintf(out, "INVOICE\n");
    fprintf(out, "Account: %d - %s (%s)\n", inv->account.id, inv->account.name, inv->account.email);
    if (inv->period[0] == '\0') {
        fprintf(out, "Period:  all time\n");
    } else {
        fprintf(out, "Period:  %s\n", inv->period);
    }
    fprintf(out, "Usage records: %d\n", inv->record_count);
    fprintf(out, "------------------------------------------------------------------\n");
    fprintf(out, "%-20s %14s %-12s %12s\n", "Meter", "Quantity", "Unit", "Charge");
    fprintf(out, "------------------------------------------------------------------\n");

    for (i = 0; i < METER_COUNT; i++) {
        fprintf(out, "%-20s %14.2f %-12s %11.2f", meters[i].name, inv->quantity[i],
                meters[i].unit, inv->charge[i]);
        if (i == METER_NET_IN) {
            fprintf(out, "  (free)");
        }
        fprintf(out, "\n");
    }

    fprintf(out, "------------------------------------------------------------------\n");
    fprintf(out, "%-20s %14s %-12s %11.2f\n", "TOTAL DUE ($)", "", "", inv->total);
    fprintf(out, "==================================================================\n");
}

/* Save an invoice to data/invoices/invoice_<account>_<period>.txt */
int invoice_write(Invoice *inv, char *path_out, int size)
{
    char name[PATH_LEN];
    FILE *f;

    if (inv->period[0] == '\0') {
        snprintf(name, PATH_LEN, "invoices/invoice_%d_all.txt", inv->account.id);
    } else {
        snprintf(name, PATH_LEN, "invoices/invoice_%d_%s.txt", inv->account.id, inv->period);
    }
    path_in_data(path_out, size, name);

    f = fopen(path_out, "w");
    if (f == NULL) return -1;

    invoice_print(inv, f);
    fclose(f);
    return 0;
}

/* ---------- summary ---------- */

/* Print one line per account for the period. */
int summary_print(char *period)
{
    Account *accounts = NULL;
    Invoice inv;
    double grand_total = 0;
    int count;
    int i;

    count = db_account_list(&accounts);
    if (count < 0) return -1;

    if (period[0] == '\0') {
        printf("\nBilling summary - all time\n");
    } else {
        printf("\nBilling summary - %s\n", period);
    }

    if (count == 0) {
        printf("No accounts.\n");
        free(accounts);
        return 0;
    }

    printf("%-4s %-20s %7s %11s %11s %10s %10s %11s\n",
           "ID", "Account", "Records", "CPU-hrs", "Mem-GBhrs", "Storage-GB", "Out-MB", "Total($)");

    for (i = 0; i < count; i++) {
        if (invoice_build(&inv, &accounts[i], period) != 0) continue;

        printf("%-4d %-20.20s %7d %11.2f %11.2f %10.0f %10.2f %11.2f\n",
               accounts[i].id, accounts[i].name, inv.record_count,
               inv.quantity[METER_CPU], inv.quantity[METER_MEM],
               inv.quantity[METER_STORAGE], inv.quantity[METER_NET_OUT], inv.total);
        grand_total = grand_total + inv.total;
    }

    printf("Grand total: %.2f\n", grand_total);
    free(accounts);
    return 0;
}

/* ---------- export ---------- */

/* Write text in quotes for CSV. A quote inside the text is doubled. */
static void write_csv_text(FILE *f, char *s)
{
    fputc('"', f);
    while (*s != '\0') {
        if (*s == '"') fputc('"', f);
        fputc(*s, f);
        s++;
    }
    fputc('"', f);
}

/* Write text in quotes for JSON. Quotes and backslashes get a backslash. */
static void write_json_text(FILE *f, char *s)
{
    fputc('"', f);
    while (*s != '\0') {
        if (*s == '"' || *s == '\\') {
            fputc('\\', f);
        }
        fputc(*s, f);
        s++;
    }
    fputc('"', f);
}

/* Export the summary of all accounts as CSV (as_json = 0) or JSON (as_json = 1).
   The file goes in data/exports. Returns 0 on success, -1 on error. */
int export_summary(char *period, int as_json, char *path_out, int size)
{
    Account *accounts = NULL;
    Invoice inv;
    FILE *f;
    char name[PATH_LEN];
    char *label;
    double grand_total = 0;
    int count;
    int written = 0;
    int i, m;

    count = db_account_list(&accounts);
    if (count < 0) return -1;

    if (period[0] == '\0') {
        label = "all";
    } else {
        label = period;
    }

    if (as_json) {
        snprintf(name, PATH_LEN, "exports/summary_%s.json", label);
    } else {
        snprintf(name, PATH_LEN, "exports/summary_%s.csv", label);
    }
    path_in_data(path_out, size, name);

    f = fopen(path_out, "w");
    if (f == NULL) {
        free(accounts);
        return -1;
    }

    if (as_json) {
        fprintf(f, "{\n  \"period\": \"%s\",\n  \"accounts\": [", label);
    } else {
        fprintf(f, "account_id,account_name,period,meter,quantity,unit,charge\n");
    }

    for (i = 0; i < count; i++) {
        if (invoice_build(&inv, &accounts[i], period) != 0) continue;
        grand_total = grand_total + inv.total;

        if (as_json) {
            if (written > 0) fprintf(f, ",");
            fprintf(f, "\n    {\"account_id\": %d, \"account_name\": ", inv.account.id);
            write_json_text(f, inv.account.name);
            fprintf(f, ", \"total\": %.2f, \"lines\": [", inv.total);
            for (m = 0; m < METER_COUNT; m++) {
                if (m > 0) fprintf(f, ", ");
                fprintf(f, "{\"meter\": \"%s\", \"quantity\": %.2f, \"unit\": \"%s\", \"charge\": %.2f}",
                        meters[m].name, inv.quantity[m], meters[m].unit, inv.charge[m]);
            }
            fprintf(f, "]}");
        } else {
            for (m = 0; m < METER_COUNT; m++) {
                fprintf(f, "%d,", inv.account.id);
                write_csv_text(f, inv.account.name);
                fprintf(f, ",%s,\"%s\",%.2f,\"%s\",%.2f\n", label,
                        meters[m].name, inv.quantity[m], meters[m].unit, inv.charge[m]);
            }
        }
        written++;
    }

    if (as_json) {
        fprintf(f, "\n  ],\n  \"grand_total\": %.2f\n}\n", grand_total);
    }

    fclose(f);
    free(accounts);
    return 0;
}
