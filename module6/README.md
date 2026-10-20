1. Problem Statement
- For this assignment I had to change the source code of two coreutils commands. ls should show the long format by default, like I typed ls -l, and whoami should print "You are: " before the username

3. Describe the Solution
- For ls.c, I found decode_switches(), which picks the output format. For plain ls the default used to be columns or one per line depending on whether output goes to a terminal. I changed that default so it's long_format instead. I didn't force long format everywhere, though. The code still checks format_opt first, so if someone passes an explicit flag like -1 or -m, that flag still wins and only the default changed. I left the quoting setup alone For whoami.c, I swapped puts (pw->pw_name); for printf("You are: %s\n", pw->pw_name);. Then I rebuilt and ran the new binaries from build/coreutils-9.7/src/

4. Pros and Cons
- Pros: It's a really small change, and since it reuses the long format code that -l already uses, the output matches ls -l exactly. Other options like ls -C still work.
- Cons: It changes what these commands do by default, so any script that expects short ls output or just the username from whoami would break. ls also gets a bit slower in big directories because it has to look up info on every file

4. Screenshot
<img width="748" height="243" alt="Screenshot 2026-10-19 at 7 57 20 PM" src="https://github.com/user-attachments/assets/eadf4cac-04e6-487c-b80d-044b3f52c32f" />
