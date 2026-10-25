Cloud Services Billing System (INET 3101 Capstone)

Demo video: 

Problem Statement:

1. What problem does this solve?

   A cloud provider has to bill customers for servers, storage and network, and each one is measured differently. My program keeps track of customer accounts and their usage, then works out what each customer owes and produces an invoice.

2. How is each service billed?

   Servers are charged on CPU hours and memory hours separately. Storage is charged per whole GB, so 250.9 GB counts as 250. Network is charged on outbound MB only, inbound is free.

Design and Build Architecture

3. How do I run it?

   It needs gcc, make and gdbm installed. Run make, then ./billing. For the TCP part, pick option 9 in the menu and start ./meter_agent in a second terminal with port 5050, an account id and a count.

4. How is the code split up?

   Six C files and one header. main.c is the menu, db.c handles gdbm, and billing.c does the pricing and invoices. server.c and meter_agent.c are the two sides of the TCP connection. util.c covers settings, logging and input checking.

5. What data structures did I use?

   Structs for Account, UsageRecord, Meter and Invoice. The usage ledger is a linked list. I treated the ledger and the invoice as the two ADTs, since each one has its own set of functions and the rest of the code only goes through those.

6. How is the data stored?

   Accounts and usage records both live in gdbm files (accounts.db and usage.db) with full create, read, update and delete. Invoices, exports and the log are plain text files. Ids come from a counter, so a deleted id never gets reused.

7. How does the TCP ingest work?

   The program listens on localhost port 5050 for one agent, for up to 60 seconds. The agent sends a line per reading and the server answers OK or an error message. Bad lines (wrong format, unknown account, negative values) are rejected rather than crashing anything.

8. How does the pricing work?

   Each meter has three tiers, so the rate drops as usage goes up. For CPU it is $0.050 per hour for the first 1000, $0.040 for the next 4000, then $0.030. The other meters follow the same idea. The rates are sample numbers I picked, and they sit in one table at the top of billing.c. Inbound network has a rate of zero.

9. What else is included?

   A log file that rotates when it gets too big, three environment settings (data folder, port, log size), a cap on open files, and exit codes (0 ok, 2 bad settings, 3 database error). The menu also has invoices, a billing summary and CSV or JSON export.

Pros and Cons:

10. What works well?

    Full CRUD through gdbm, and the billing rules follow the assignment. Rates are kept in one place so they are easy to change. Input is checked almost everywhere, and the code is split up by job instead of sitting in one big file.

11. What are the weak spots?

    Records are saved as raw structs, so the database files would not move cleanly between different machines. Every invoice reads all the usage and filters it in memory. That is fine at this size but it would not scale. The server only handles one agent at a time, and deleting an account does not ask for confirmation. There are also no automated tests, everything was checked by running the program by hand. If I kept working on it, loading the rates from a file would be the first thing I would change.
