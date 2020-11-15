#include <iostream>
#include <fstream>
#include <string>
#include <windows.h>
#include <sstream>
#include <time.h>

using namespace std;

const string version = "\n --- WireBox v2.0. Written by Shpati Koleka, November 2020. --- \n\n";

const int row = 958;
const int col = 631;

char arr[row][col];
string file_in = "input.txt";
string file_out = "output";
int cyc_num = 1; 
int ms = 0;
string printsel = "L";
bool print = false, print_term = false, print_txt = false, print_pgm = false;

void read_file(string file_in) {

ifstream myfile;
int i, j;

myfile.open(file_in.c_str());
if(!myfile)
	{
		cout << "No input file found, exiting program! Use -help parameter for help." << endl; 
		exit(0);
	} 
for (i = 0; i < row; i++)
	{
		for (j = 0; j < col; j++)
		{
			myfile >> arr[i][j];
		}
	}
myfile.close();
}

void print_terminal(int k) {
system("CLS");
cout << "\rCycle #" << k;
cout << "\n";
int i, j;
for (i = 0; i < row; i++)
	{
		for (j = 0; j < col; j++)
		{
			cout << arr[i][j];
		}
		cout << "\n";
	}
cout << "\n";
}

int electrons_around(int r, int c) {

string s;
int count = 0;
	s += arr[r-1][c-1];
	s += arr[r][c-1]; 
	s += arr[r+1][c-1];
	s += arr[r-1][c]; 
	s += arr[r+1][c];
	s += arr[r-1][c+1];
	s += arr[r][c+1];
	s += arr[r+1][c+1];
	for (int k = 0; k < s.size(); k++)
		if (s[k] == 'e') count++;
return count;
}

void cycle() {

int i, j;
char arr_new[row][col] = { 0 };

for (i = 1; i < row-1; i++)
	{
		for (j = 1; j < col-1; j++)
		{
			if (arr[i][j] == 'e') {
				arr_new[i][j] = '~';
			} else if(arr[i][j] == '~') {
				arr_new[i][j] = '#';
			} else if (arr[i][j] == '#') {
				if (electrons_around(i,j) == 1 || electrons_around(i,j) == 2) {
					arr_new[i][j] = 'e';
					} else {
					arr_new[i][j] = '#';
					}
			}
		}
	}
for (i = 0; i < row; i++)
	{
		for (j = 0; j < col; j++)
		{
				arr[i][j] = arr_new[i][j];
		}
	}
}

void write_txt(string file_out_txt) {

ofstream myfile;
int i, j;

myfile.open(file_out_txt.c_str());
if(!myfile)
	{
		cout << "Output file could not be created!" << endl; 
		exit(0);
	} 
for (i = 0; i < row; i++)
	{
		for (j = 0; j < col; j++)
		{
			myfile << arr[i][j];
		}
	myfile << "\n";
	}
myfile.close();
}


void write_pgm(string file_out_pgm) {

ofstream myfile;
int i, j;

myfile.open(file_out_pgm.c_str());
if(!myfile)
	{
		cout << "Output file could not be created!" << endl; 
		exit(0);
	} 

myfile << "P5 631 958 255 ";

for (i = 0; i < row; i++)
	{
		for (j = 0; j < col; j++)
		{
			//if (i== 0 && j < 16) {
			//} else { 
				if (arr[i][j] != '\n') {
				myfile << arr[i][j];
			//	}
			}
		}
	}
myfile.close();
}

void print_array(string file_out, int i) {
stringstream txt_file;
stringstream pgm_file;
string txt_f;
string pgm_f;

txt_file << file_out << "_" << i << ".txt";
pgm_file << file_out << "_" << i << ".pgm";


txt_file >> txt_f;
pgm_file >> pgm_f;

if (print_term == true) print_terminal(i);
if (print_txt == true) write_txt(txt_f);
if (print_pgm == true) write_pgm(pgm_f);
}


void play(int num, int ms, string printsel, bool print) {

int i = 0;
if (printsel == "L") {
	while (num > i) {
		cycle();
		i++;
		cout << "\rCycle #" << i;
		}
	while (num == 0) {
		cycle();
		i++;
		cout << "\rCycle #" << i;
		}
	} else {
	cout << "\rInput";
	while (num > i || num == 0) {
		if (print == true) print_array(file_out,i);
		cycle();
		i++;
		cout << "\rCycle #" << i;
		Sleep(ms);
		}
	} 
print_txt = true;
print_pgm = true;
print_array(file_out,i);
}


void help() {
system("CLS");
cout << version;
cout << "Usage: wireworld [-i filename] [-o filename] [-scr] [-txt] [-pgm]  \n";
cout << "                 [-d milliseconds] [-n cycles] [-a] [-r] [-h] [-v] \n";
cout << "                                                                   \n";
cout << "Options:                                                           \n";
cout << "                                                                   \n";
cout << "  -i filename    Specify the text-input filename. If not used, the \n";
cout << "                 program looks for a file called 'input.txt' in    \n";
cout << "                 the same directory as the program.                \n";
cout << "  -o filename    Specify the output filename.If not used, the      \n";
cout << "                 program creates a file called 'output' in         \n";
cout << "                 the same directory as the program.                \n";
cout << "  -scr           Print the results to screen. Disabled by default. \n";
cout << "  -txt           Print the results to text file. Disabled by       \n";
cout << "                 default, except for the last cycle (end result).  \n";
cout << "  -pgm           Print the results to a pgm (grayscale image) file.\n";
cout << "                 Disabled by default, except for the last cycle.   \n";
cout << "  -d millisecs   The amount of milliseconds that the program waits \n";
cout << "                 between each cycle. The default value is zero.    \n";
cout << "  -n cycles      Number of cycles to run. If not used, the default \n";
cout << "                 value is 1. If assigned 0, it will cycle forever. \n";
cout << "  -a             Display all cycles. If not used the program only  \n";
cout << "                 shows the result of the last cycle defined by n.  \n";
cout << "                 Use it in combination with [-scr], [-txt], [-pgm] \n";
cout << "  -h             Displays the help.                                \n";
cout << "  -v             Displays the program version.                     \n";
cout << "                                                                   \n";
cout << "WireBox uses the following symbols:                                \n";
cout << "                                                                   \n";
cout << " e = electron head   (hex code: 65)                                \n";
cout << " ~ = electron tail   (hex code: 7E)                                \n";
cout << " # = wire            (hex code: 23)                                \n";
cout << " . = blank           (hex code: 00)                                \n";
cout << "                                                                   \n";
cout << "General notes:                                                     \n";
cout << "                                                                   \n";
cout << "                 - The program execution can be stopped anytime by \n";
cout << "                   pressing Control-C                              \n";
cout << "                 - Leave the contours/edges of the array blank.    \n";
cout << "                                                                   \n";
}

void main(int argc, char *argv[]) {

// Command-line arguments.
for (int count = 1; count < argc; count++) {
	
	std::string word = argv[count];
	if (word == "-h") {
		help();
		exit(0);
	} else if (word == "-v") {
		cout << version; 
		exit(0);
	} else if (word == "-i") {
		std::string word = argv[count+1];
		file_in = word;
	} else if (word == "-o") {
		std::string word = argv[count+1];
		file_out = word;
	} else if (word == "-scr") {
		print_term = true;
		print = true;
	} else if (word == "-pgm") {
		print_pgm = true;
		print = true;
	} else if (word == "-txt") {
		print_txt = true;
		print = true;
	} else if (word == "-d") {
		std::string str = argv[count+1];
		std::istringstream(str) >> ms;
	} else if (word == "-n") {
		std::string str = argv[count+1];
		std::istringstream(str) >> cyc_num;
	} else if (word == "-a") {
		printsel = "A";
		print = true;
	}
}


system("CLS");
read_file(file_in);

time_t start_time = time(NULL);

play(cyc_num, ms, printsel, print);

time_t end_time1 = time(NULL);
time_t result = end_time1 - start_time;
cout << "\nDuration: " << result << " seconds"<<endl;
 
}