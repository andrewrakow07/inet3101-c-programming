# Cloud Services Billing System

INET 3101 Capstone Project (Cloud Services Billing topic)

**Demo video:** PASTE YOUR PUBLIC VIDEO LINK HERE

## Problem Statement

A cloud provider offers three services: Servers, Storage and Network. Each one is measured differently and has to be billed to the customer who used it:

- Servers are metered on both CPU (vCPU-hours) and memory (GB-hours).
- Storage is metered in GB. Anything fractional is rounded down.
- Network is metered in MB, both inbound and outbound. Inbound traffic is free.

This application is a record management and billing system written in C. It stores customer accounts and their metered usage in a gdbm database with full create, read, update and delete (CRUD) support. It ingests usage readings from a mock metering agent over TCP, applies tiered pricing, and produces invoices, billing summaries and CSV/JSON exports.

## Design and Architecture Details

### Build and run

Requires gcc, make and the gdbm library (`sudo apt install libgdbm-dev` on Ubuntu, `brew install gdbm` on macOS).

```
make
./billing                          # the billing system (menu)
./meter_agent <port> <account_id> <count>   # the mock metering agent
```

To ingest usage: choose menu option 9 in `./billing`, then in a second terminal run `./meter_agent 5050 <account_id> <count>`.

### Source files

| File | Purpose |
|------|---------|
| `billing.h` | Constants, structs, ADT interfaces and all function prototypes |
| `main.c` | Menu and user interface |
| `db.c` | gdbm persistence layer (CRUD for accounts and usage records) |
| `billing.c` | Pricing tiers, Usage Ledger ADT, Invoice ADT, summary, CSV/JSON export |
| `server.c` | TCP ingest server |
| `meter_agent.c` | Standalone mock metering agent (TCP client) |
| `util.c` | Environment config, append-only log with rotation, resource limits, input helpers |
| `Makefile` | Builds `billing` and `meter_agent` |

### Key data structures

- `Account`: id, name, email, creation time.
- `UsageRecord`: id, account id, resource type, timestamp, and the measured values (`cpu_hours`, `mem_gb_hours`, `storage_gb`, `net_in_mb`, `net_out_mb`). Only the fields matching the resource type are used.
- `Meter`: a billable item (Server CPU, Server Memory, Storage, Network Outbound, Network Inbound) with its unit and three price tiers (an upper limit and a rate per unit for each tier).

### Abstract data types

- **Usage Ledger** (`Ledger`, a singly linked list). Usage records for one account and billing period are loaded into the ledger, which totals the billable quantity per meter. Operations: `ledger_init`, `ledger_add`, `ledger_totals`, `ledger_free`. Storage is rounded down per record when totaled.
- **Invoice** (`Invoice`). Built from a ledger plus the pricing tiers (`invoice_build`), printed with `invoice_print`, and saved to a file with `invoice_write`. It has one line per meter and a total.

### Storage

Two gdbm files in the data directory: `accounts.db` and `usage.db`. Each record is stored as its struct bytes under its id as the key. A `next_id` key in each file hands out ids that are never reused. Each database operation opens the file, does its work and closes it, which avoids gdbm lock conflicts.

Other files in the data directory:

- `billing.log`: append-only event log. When it reaches the size limit it rotates to `billing.log.1`, `.2`, `.3`.
- `invoices/`: one text file per generated invoice.
- `exports/`: CSV and JSON summary exports.

### TCP ingest protocol

The billing system listens on `127.0.0.1` for one agent connection (menu option 9, 60 second wait). The agent sends one text line per reading and the server replies to each:

```
USAGE <account_id> SERVER  <cpu_hours> <mem_gb_hours>
USAGE <account_id> STORAGE <gb>
USAGE <account_id> NETWORK <inbound_mb> <outbound_mb>
QUIT
```

Replies are `OK <usage_id>`, `ERR <reason>` or `BYE`. The server validates the format, resource, value range and that the account exists.

### Pricing tiers (sample rates)

| Meter | Tier 1 | Tier 2 | Tier 3 |
|-------|--------|--------|--------|
| Server CPU (per vCPU-hour) | first 1,000 at $0.050 | next 4,000 at $0.040 | above 5,000 at $0.030 |
| Server Memory (per GB-hour) | first 5,000 at $0.010 | next 15,000 at $0.008 | above 20,000 at $0.006 |
| Storage (per GB) | first 1,000 at $0.020 | next 9,000 at $0.015 | above 10,000 at $0.010 |
| Network Outbound (per MB) | first 10,000 at $0.0010 | next 90,000 at $0.0008 | above 100,000 at $0.0006 |
| Network Inbound | free | | |

Each invoice line is rounded to cents.

### Configuration, limits and exit codes

- Environment variables: `BILLING_DATA_DIR` (default `data`), `BILLING_PORT` (default 5050), `BILLING_LOG_MAX_BYTES` (default 1048576).
- Resource limits: open file descriptors capped at 256 and core dumps disabled (`setrlimit`). Input lines and protocol lines have fixed maximum lengths and values are range checked.
- Exit codes for `billing`: 0 normal, 2 invalid configuration, 3 database could not be opened. For `meter_agent`: 0 ok, 1 bad arguments, 4 could not connect, 5 server closed early.

### Menu features

1. Create account, 2. View accounts, 3. Update account, 4. Delete account (also removes its usage)
5. Add usage record, 6. View usage records, 7. Update usage record, 8. Delete usage record
9. Receive usage from metering agent (TCP)
10. Generate invoice (all time or one `YYYY-MM` period, printed and saved to a file)
11. Billing summary (all accounts)
12. Export summary to CSV or JSON

## Pros and Cons

### Pros

- Full CRUD on both accounts and usage records, persisted in gdbm.
- Clear separation of concerns: UI, database layer, billing logic and networking each have their own file.
- Billing rules follow the brief exactly: CPU and memory metered, storage rounded down, inbound network free.
- Tiered pricing is data driven (a table of meters), so rates are easy to change in one place.
- Input handling is defensive: all numbers and dates are validated, long input lines are handled, and the server rejects malformed or out-of-range messages without crashing.
- Includes a log that rotates, environment configuration, resource limits and meaningful exit codes.

### Cons and future improvements

- Records are stored as raw struct bytes, so the database files are not portable between machines with different architectures or compilers. A text or serialized format would fix this.
- Listing and invoicing read every record and filter in memory. This is fine for a course project but would not scale to very large ledgers; secondary indexes by account would help.
- The ingest server handles one agent at a time and listens on localhost only. Handling several agents at once would need threads or `select`/`poll`.
- Rates are compiled into the program. Loading them from a configuration file would allow changes without recompiling.
- The account and usage functions in `db.c` repeat similar code. A shared helper would shorten it.
- Account deletion removes the account's usage without asking for confirmation.
- There are no automated tests; the program was verified with scripted manual runs.
