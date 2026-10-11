1. Problem Statement
Write a server and client in C using TCP sockets. The client asks for a file by name. The server sends back the file’s text, which can be at most 2048 characters. The client prints what it gets back. The programs must handle an invalid file name, a file that is not found, and a file that is too large. Bonus: the server should shut down gracefully when it is told to terminate.

2. Describe the Solution
The server creates a socket on port 9001, binds, listens, and loops forever accepting one client at a time. It reads the file name from the client and checks it. A name that is empty or contains a slash or “..” is rejected as invalid. If the file cannot be opened, it sends back a file not found error. It reads up to 2049 characters from the file, and if it gets more than 2048 it sends back a file too large error. Otherwise it sends “OK” followed by the file contents. The client asks the user to type a file name, connects to the server, sends the name, and reads the response. If the response starts with “OK” it prints the file contents. Otherwise it prints the error message. For graceful shutdown, the server uses signal() to catch Ctrl+C (SIGINT) and kill (SIGTERM). The handler prints a shutdown message, closes the server socket, and exits. 

3. Pros and Cons
- Pros: simple and easy to understand, handles all three required errors, blocks file names that try to leave the current folder, and shuts down cleanly.
- Cons: it serves only one client at a time, the port is fixed, there is no encryption or login, and it only works with text files. The file name is read with a single recv call, which assumes it arrives all at once.

4. Screenshot
<img width="1197" height="432" alt="Screenshot 2026-10-11 at 2 17 56 PM" src="https://github.com/user-attachments/assets/2e3868e8-0fee-4351-9e64-787ca96ca3ca" />
