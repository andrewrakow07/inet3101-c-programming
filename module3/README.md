1. Problem Statement
Colossus Airlines has one plane with 128 seats. It flies 4 flights a day, 2 outbound and 2 inbound. We had to write a seat reservation program in C. It needs an array of structs for the seats and an array for the 4 flight numbers. Each seat has an id, a marker for whether it’s taken, and the first and last name of the person in it. The program has three menus. The first one is outbound, inbound, or quit. The second one is flight number or back to main. The third one lets you see how many seats are empty, list the empty seats, list the assigned seats alphabetically, assign a customer to a seat, delete an assignment, or go back to main. Assign and delete have to let you abort. After each choice the menu shows again, except return to main, which goes back to the first menu.

3. Describe the Solution
I made a struct called Seat with an id, an assigned marker (0 is empty, 1 is taken), a last name, and a first name. The seats are in a 2D array, seats[4][128], so each flight gets its own 128 seats. The flight numbers are in an array called flightNumbers: 101, 102, 201, 202. The lab didn’t say which ones are outbound or inbound, so I picked 101 and 102 for outbound and 201 and 202 for inbound. Each menu has its own function. main shows the first menu and calls secondMenu for outbound or inbound. secondMenu asks for a flight number and checks that it’s one of the two flights for that direction. If it is, thirdMenu runs for that flight. Choosing f in the third menu goes back to the first menu. For the alphabetical list I copy the taken seats into a temp array and bubble sort the copy by last name, then first name. That way the real seat order doesn’t get messed up. For assign and delete, typing q at any prompt cancels it.

3. Pros and Cons of your solution 
Pros: It’s simple and only uses basic C stuff. The arrays are a fixed size so there’s no memory to manage. Each flight has its own seats so they don’t affect each other. Sorting a copy keeps the seat order the same. It handles bad menu letters, bad flight numbers, and bad seat numbers, and you can abort when assigning or deleting.

Cons: Nothing is saved when the program closes. The flight numbers are hardcoded. Names can only be one word (no spaces) and 29 characters max. Menu choices have to be lowercase. Bubble sort would be slow for big lists, but it’s fine for 128 seats.

Screenshots (or a link to the screen video recording) that shows program’s inputs and outputs
<img width="680" height="667" alt="Screenshot 2026-10-08 at 2 01 02 PM" src="https://github.com/user-attachments/assets/7b1371ba-a36b-4356-8489-1595ef741f2a" />
<img width="434" height="612" alt="Screenshot 2026-10-08 at 2 01 10 PM" src="https://github.com/user-attachments/assets/19febdb1-c6a4-4840-bc02-0a0698089b86" />

