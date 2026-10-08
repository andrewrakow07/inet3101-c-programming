/* Colossus Airlines seat reservation program */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define FLIGHTS 4
#define SEATS 128

struct Seat
{
    int id;       /* seat number 1 to 128 */
    int assigned; /* 0 = empty, 1 = assigned */
    char last[30];
    char first[30];
};

/* flights 0 and 1 are outbound, flights 2 and 3 are inbound */
int flightNumbers[FLIGHTS] = {101, 102, 201, 202};

/* seats[flight][seat] */
struct Seat seats[FLIGHTS][SEATS];

/* make every seat empty */
void setup()
{
    int f, s;
    for (f = 0; f < FLIGHTS; f++)
    {
        for (s = 0; s < SEATS; s++)
        {
            seats[f][s].id = s + 1;
            seats[f][s].assigned = 0;
            strcpy(seats[f][s].last, "");
            strcpy(seats[f][s].first, "");
        }
    }
}

/* a) show number of empty seats */
void countEmpty(int f)
{
    int s;
    int count = 0;
    for (s = 0; s < SEATS; s++)
    {
        if (seats[f][s].assigned == 0)
        {
            count++;
        }
    }
    printf("Number of empty seats: %d\n", count);
}

/* b) show list of empty seats */
void listEmpty(int f)
{
    int s;
    printf("Empty seats:\n");
    for (s = 0; s < SEATS; s++)
    {
        if (seats[f][s].assigned == 0)
        {
            printf("%d ", seats[f][s].id);
        }
    }
    printf("\n");
}

/* c) show alphabetical list of seats (sorts a copy) */
void alphabetical(int f)
{
    struct Seat copy[SEATS];
    struct Seat temp;
    int s, i, j;
    int n = 0;

    /* copy the assigned seats */
    for (s = 0; s < SEATS; s++)
    {
        if (seats[f][s].assigned == 1)
        {
            copy[n] = seats[f][s];
            n++;
        }
    }

    if (n == 0)
    {
        printf("No seats are assigned.\n");
        return;
    }

    /* bubble sort by last name, then first name */
    for (i = 0; i < n - 1; i++)
    {
        for (j = 0; j < n - 1 - i; j++)
        {
            int result = strcmp(copy[j].last, copy[j + 1].last);
            if (result == 0)
            {
                result = strcmp(copy[j].first, copy[j + 1].first);
            }
            if (result > 0)
            {
                temp = copy[j];
                copy[j] = copy[j + 1];
                copy[j + 1] = temp;
            }
        }
    }

    for (i = 0; i < n; i++)
    {
        printf("Seat %d: %s, %s\n", copy[i].id, copy[i].last, copy[i].first);
    }
}

/* d) assign a customer to a seat (type q to abort) */
void assignSeat(int f)
{
    char text[30];
    char first[30];
    char last[30];
    int seat;

    printf("Enter seat number (1-128) or q to abort: ");
    scanf("%29s", text);
    if (text[0] == 'q')
    {
        printf("Aborted.\n");
        return;
    }
    seat = atoi(text);
    if (seat < 1 || seat > SEATS)
    {
        printf("Invalid seat number.\n");
        return;
    }
    if (seats[f][seat - 1].assigned == 1)
    {
        printf("That seat is already taken.\n");
        return;
    }

    printf("Enter first name or q to abort: ");
    scanf("%29s", first);
    if (strcmp(first, "q") == 0)
    {
        printf("Aborted.\n");
        return;
    }

    printf("Enter last name or q to abort: ");
    scanf("%29s", last);
    if (strcmp(last, "q") == 0)
    {
        printf("Aborted.\n");
        return;
    }

    seats[f][seat - 1].assigned = 1;
    strcpy(seats[f][seat - 1].first, first);
    strcpy(seats[f][seat - 1].last, last);
    printf("Seat %d assigned to %s %s.\n", seat, first, last);
}

/* e) delete a seat assignment (type q to abort) */
void deleteSeat(int f)
{
    char text[30];
    int seat;

    printf("Enter seat number to delete (1-128) or q to abort: ");
    scanf("%29s", text);
    if (text[0] == 'q')
    {
        printf("Aborted.\n");
        return;
    }
    seat = atoi(text);
    if (seat < 1 || seat > SEATS)
    {
        printf("Invalid seat number.\n");
        return;
    }
    if (seats[f][seat - 1].assigned == 0)
    {
        printf("That seat is not assigned.\n");
        return;
    }

    seats[f][seat - 1].assigned = 0;
    strcpy(seats[f][seat - 1].first, "");
    strcpy(seats[f][seat - 1].last, "");
    printf("Seat %d is now empty.\n", seat);
}

/* third level menu */
void thirdMenu(int f)
{
    char choice;

    do
    {
        printf("\nFlight %d\n", flightNumbers[f]);
        printf("To choose a function, enter its letter label:\n");
        printf("a) Show number of empty seats\n");
        printf("b) Show list of empty seats\n");
        printf("c) Show alphabetical list of seats\n");
        printf("d) Assign a customer to a seat assignment\n");
        printf("e) Delete a seat assignment\n");
        printf("f) Return to Main menu\n");
        scanf(" %c", &choice);

        if (choice == 'a')
        {
            countEmpty(f);
        }
        else if (choice == 'b')
        {
            listEmpty(f);
        }
        else if (choice == 'c')
        {
            alphabetical(f);
        }
        else if (choice == 'd')
        {
            assignSeat(f);
        }
        else if (choice == 'e')
        {
            deleteSeat(f);
        }
        else if (choice != 'f')
        {
            printf("Invalid choice.\n");
        }
    } while (choice != 'f');
}

/* second level menu: direction 0 = outbound, 1 = inbound */
void secondMenu(int direction)
{
    char choice;
    char text[30];
    int number, i;
    int found;
    int start = direction * 2;

    do
    {
        printf("\nTo choose a function, enter its letter label:\n");
        printf("a) Flight Number:\n");
        printf("b) Back to Main\n");
        scanf(" %c", &choice);

        if (choice == 'a')
        {
            printf("Valid flights: %d and %d\n",
                   flightNumbers[start], flightNumbers[start + 1]);
            printf("Enter flight number: ");
            scanf("%29s", text);
            number = atoi(text);

            found = -1;
            for (i = start; i < start + 2; i++)
            {
                if (flightNumbers[i] == number)
                {
                    found = i;
                }
            }

            if (found == -1)
            {
                printf("Invalid flight number.\n");
            }
            else
            {
                thirdMenu(found);
                return; /* f goes back to the first menu */
            }
        }
        else if (choice != 'b')
        {
            printf("Invalid choice.\n");
        }
    } while (choice != 'b');
}

/* first level menu */
int main()
{
    char choice;

    setup();

    do
    {
        printf("\nTo choose a function, enter its letter label:\n");
        printf("a) Outbound Flight\n");
        printf("b) Inbound Flight\n");
        printf("c) Quit\n");
        scanf(" %c", &choice);

        if (choice == 'a')
        {
            secondMenu(0);
        }
        else if (choice == 'b')
        {
            secondMenu(1);
        }
        else if (choice != 'c')
        {
            printf("Invalid choice.\n");
        }
    } while (choice != 'c');

    printf("Goodbye.\n");
    return 0;
}
