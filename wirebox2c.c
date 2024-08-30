#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#define ROW 958
#define COL 631

const char *version = "\n --- WireBox v2.0. Written by Shpati Koleka, November 2020. --- \n\n";

char arr[ROW][COL];
char file_in[100] = "input.txt";
char file_out[100] = "output";
int cyc_num = 1;
int ms = 0;
char printsel[2] = "L";
int print = 0, print_term = 0, print_txt = 0, print_pgm = 0;

void read_file(const char *file_in) {
    FILE *myfile;
    int i, j;

    myfile = fopen(file_in, "r");
    if (!myfile) {
        printf("No input file found, exiting program! Use -help parameter for help.\n");
        exit(0);
    }
    for (i = 0; i < ROW; i++) {
        for (j = 0; j < COL; j++) {
            fscanf(myfile, " %c", &arr[i][j]);
        }
    }
    fclose(myfile);
}

void print_terminal(int k) {
    system("CLS");
    printf("\rCycle #%d\n", k);
    int i, j;
    for (i = 0; i < ROW; i++) {
        for (j = 0; j < COL; j++) {
            printf("%c", arr[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}

int electrons_around(int r, int c) {
    int count = 0;
    if (arr[r-1][c-1] == 'e') count++;
    if (arr[r][c-1] == 'e') count++;
    if (arr[r+1][c-1] == 'e') count++;
    if (arr[r-1][c] == 'e') count++;
    if (arr[r+1][c] == 'e') count++;
    if (arr[r-1][c+1] == 'e') count++;
    if (arr[r][c+1] == 'e') count++;
    if (arr[r+1][c+1] == 'e') count++;
    return count;
}

void cycle() {
    int i, j;
    char arr_new[ROW][COL] = { 0 };

    for (i = 1; i < ROW-1; i++) {
        for (j = 1; j < COL-1; j++) {
            if (arr[i][j] == 'e') {
                arr_new[i][j] = '~';
            } else if (arr[i][j] == '~') {
                arr_new[i][j] = '#';
            } else if (arr[i][j] == '#') {
                int around = electrons_around(i, j);
                if (around == 1 || around == 2) {
                    arr_new[i][j] = 'e';
                } else {
                    arr_new[i][j] = '#';
                }
            }
        }
    }
    for (i = 0; i < ROW; i++) {
        for (j = 0; j < COL; j++) {
            arr[i][j] = arr_new[i][j];
        }
    }
}

void write_txt(const char *file_out_txt) {
    FILE *myfile;
    int i, j;

    myfile = fopen(file_out_txt, "w");
    if (!myfile) {
        printf("Output file could not be created!\n");
        exit(0);
    }
    for (i = 0; i < ROW; i++) {
        for (j = 0; j < COL; j++) {
            fprintf(myfile, "%c", arr[i][j]);
        }
        fprintf(myfile, "\n");
    }
    fclose(myfile);
}

void write_pgm(const char *file_out_pgm) {
    FILE *myfile;
    int i, j;

    myfile = fopen(file_out_pgm, "w");
    if (!myfile) {
        printf("Output file could not be created!\n");
        exit(0);
    }

    fprintf(myfile, "P5\n%d %d\n255\n", COL, ROW);

    for (i = 0; i < ROW; i++) {
        for (j = 0; j < COL; j++) {
            if (arr[i][j] != '\n') {
                fprintf(myfile, "%c", arr[i][j]);
            }
        }
    }
    fclose(myfile);
}

void print_array(const char *file_out, int i) {
    char txt_f[200], pgm_f[200];

    sprintf(txt_f, "%s_%d.txt", file_out, i);
    sprintf(pgm_f, "%s_%d.pgm", file_out, i);

    if (print_term) print_terminal(i);
    if (print_txt) write_txt(txt_f);
    if (print_pgm) write_pgm(pgm_f);
}

void play(int num, int ms, const char *printsel, int print) {
    int i = 0;
    if (strcmp(printsel, "L") == 0) {
        while (num > i || num == 0) {
            cycle();
            i++;
            printf("\rCycle #%d", i);
        }
    } else {
        printf("\rInput");
        while (num > i || num == 0) {
            if (print) print_array(file_out, i);
            cycle();
            i++;
            printf("\rCycle #%d", i);
            Sleep(ms);
        }
    }
    print_txt = 1;
    print_pgm = 1;
    print_array(file_out, i);
}

void help() {
    system("CLS");
    printf("%s", version);
    printf("Usage: wireworld [-i filename] [-o filename] [-scr] [-txt] [-pgm]  \n");
    printf("                 [-d milliseconds] [-n cycles] [-a] [-r] [-h] [-v] \n");
    printf("                                                                   \n");
    printf("Options:                                                           \n");
    printf("                                                                   \n");
    printf("  -i filename    Specify the text-input filename. If not used, the \n");
    printf("                 program looks for a file called 'input.txt' in    \n");
    printf("                 the same directory as the program.                \n");
    printf("  -o filename    Specify the output filename. If not used, the     \n");
    printf("                 program creates a file called 'output' in         \n");
    printf("                 the same directory as the program.                \n");
    printf("  -scr           Print the results to screen. Disabled by default. \n");
    printf("  -txt           Print the results to text file. Disabled by       \n");
    printf("                 default, except for the last cycle (end result).  \n");
    printf("  -pgm           Print the results to a pgm (grayscale image) file.\n");
    printf("                 Disabled by default, except for the last cycle.   \n");
    printf("  -d millisecs   The amount of milliseconds that the program waits \n");
    printf("                 between each cycle. The default value is zero.    \n");
    printf("  -n cycles      Number of cycles to run. If not used, the default \n");
    printf("                 value is 1. If assigned 0, it will cycle forever. \n");
    printf("  -a             Display all cycles. If not used the program only  \n");
    printf("                 shows the result of the last cycle defined by n.  \n");
    printf("                 Use it in combination with [-scr], [-txt], [-pgm] \n");
    printf("  -h             Displays the help.                                \n");
    printf("  -v             Displays the program version.                     \n");
    printf("                                                                   \n");
    printf("WireBox uses the following symbols:                                \n");
    printf("                                                                   \n");
    printf(" e = electron head   (hex code: 65)                                \n");
    printf(" ~ = electron tail   (hex code: 7E)                                \n");
    printf(" # = wire            (hex code: 23)                                \n");
    printf(" . = blank           (hex code: 00)                                \n");
    printf("                                                                   \n");
    printf("General notes:                                                     \n");
    printf("                                                                   \n");
    printf("                 - The program execution can be stopped anytime by \n");
    printf("                   pressing Control-C                              \n");
    printf("                 - Leave the contours/edges of the array blank.    \n");
    printf("                                                                   \n");
}

int main(int argc, char *argv[]) {
    int count;
    for (count = 1; count < argc; count++) {
        if (strcmp(argv[count], "-h") == 0) {
            help();
            exit(0);
        } else if (strcmp(argv[count], "-v") == 0) {
            printf("%s", version);
            exit(0);
        } else if (strcmp(argv[count], "-i") == 0) {
            strcpy(file_in, argv[count+1]);
        } else if (strcmp(argv[count], "-o") == 0) {
            strcpy(file_out, argv[count+1]);
        } else if (strcmp(argv[count], "-scr") == 0) {
            print_term = 1;
            print = 1;
        } else if (strcmp(argv[count], "-pgm") == 0) {
            print_pgm = 1;
            print = 1;
        } else if (strcmp(argv[count], "-txt") == 0) {
            print_txt = 1;
            print = 1;
        } else if (strcmp(argv[count], "-d") == 0) {
            ms = atoi(argv[count+1]);
        } else if (strcmp(argv[count], "-n") == 0) {
            cyc_num = atoi(argv[count+1]);
        } else if (strcmp(argv[count], "-a") == 0) {
            strcpy(printsel, "A");
            print = 1;
        }
    }

    system("CLS");
    read_file(file_in);

    time_t start_time = time(NULL);

    play(cyc_num, ms, printsel, print);

    time_t end_time1 = time(NULL);
    time_t result = end_time1 - start_time;
    printf("\nDuration: %ld seconds\n", result);

    return 0;
}
