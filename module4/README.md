1. Problem Statement
The Module 3 reservation program for Colossus Airlines lost all its data when it closed. For this module, I had to extend it so reservations are saved to a file and loaded again when the program restarts.

2. Describe the Solution
The program saves the seat data to a text file called reservations.txt. Each line is one assigned seat with the flight number index, seat index, first name, and last name. The saveData function writes every assigned seat to the file, and it runs after each assign and delete. The loadData function runs at startup, reads the file, and fills the seats array. If the file doesn’t exist yet, the program starts with all seats empty. The rest of the program works the same as Module 3.

3. Pros and Cons
- Pros: The data is kept after the program closes. The file is plain text, so it’s easy to read and check. The code is simple and uses only basic file functions.
- Cons: The whole file is rewritten every time a seat changes. Names can only be one word, with no spaces. The file name is hardcoded and the program has to be run from the same folder to find it. If someone edits the file by hand incorrectly, the data could load wrong.

4. Screenshots
