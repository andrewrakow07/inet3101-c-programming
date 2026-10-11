1. Problem Statement
Implement a TCP server and client in C. The client sends a file name to the server. The server replies with the file’s text contents, which must not exceed 2048 characters. The client prints the response. Both programs must handle errors such as an invalid file name, a file that does not exist, and a file that is too large. Bonus: the server shuts down gracefully when it receives a termination signal.

2. Describe the Solution
The server creates a TCP socket on port 8080, binds, listens, and handles one client at a time in a loop. For each client it reads the file name and validates it: it must not be empty and must not contain slashes or “..”, so only files in the server’s working directory can be read. It then opens the file and reads up to 2049 bytes. If more than 2048 bytes come back, it reports that the file is too large. Otherwise it sends “OK” followed by the file contents. Errors are sent as “ERROR: <message>“. The client takes an optional server IP and file name on the command line, or prompts for the file name. It connects, sends the name, reads the full response, and prints it. If the response starts with “OK”, the contents go to standard output. Otherwise the error goes to standard error and the client exits with a non-zero code. For graceful shutdown, the server installs a handler for SIGINT and SIGTERM that sets a flag. This makes the blocked accept() call return, the loop exits, the listening socket is closed, and the server ends cleanly.

3. Pros and Cons
- Pros: the protocol is simple and easy to read, error messages are clear, file name validation blocks path traversal, all three required error cases are handled, and the server shuts down cleanly on Ctrl+C or kill.
- Cons: the server handles one client at a time, so a slow client blocks others. The file name is read with a single recv(), which assumes it arrives in one piece. The port is fixed and there is no encryption or authentication. It only supports text files, and the “OK” marker is part of the data stream instead of a proper length header.

4. Screenshot
<img width="1197" height="432" alt="Screenshot 2026-10-11 at 2 17 56 PM" src="https://github.com/user-attachments/assets/2e3868e8-0fee-4351-9e64-787ca96ca3ca" />
