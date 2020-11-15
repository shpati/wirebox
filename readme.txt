
 --- WireBox v2.0. Written by Shpati Koleka, November 2020. --- 

Usage: wireworld [-i filename] [-o filename] [-scr] [-txt] [-pgm]  
                 [-d milliseconds] [-n cycles] [-a] [-r] [-h] [-v] 
                                                                   
Options:                                                           
                                                                   
  -i filename    Specify the text-input filename. If not used, the 
                 program looks for a file called 'input.txt' in    
                 the same directory as the program.                
  -o filename    Specify the output filename.If not used, the      
                 program creates a file called 'output' in         
                 the same directory as the program.                
  -scr           Print the results to screen. Disabled by default. 
  -txt           Print the results to text file. Disabled by       
                 default, except for the last cycle (end result).  
  -pgm           Print the results to a pgm (grayscale image) file.
                 Disabled by default, except for the last cycle.   
  -d millisecs   The amount of milliseconds that the program waits 
                 between each cycle. The default value is zero.    
  -n cycles      Number of cycles to run. If not used, the default 
                 value is 1. If assigned 0, it will cycle forever. 
  -a             Display all cycles. If not used the program only  
                 shows the result of the last cycle defined by n.  
                 Use it in combination with [-scr], [-txt], [-pgm] 
  -h             Displays the help.                                
  -v             Displays the program version.                     
                                                                   
WireBox uses the following symbols:                                
                                                                   
 e = electron head   (hex code: 65)                                
 ~ = electron tail   (hex code: 7E)                                
 # = wire            (hex code: 23)                                
 . = blank           (hex code: 00)                                
                                                                   
General notes:                                                     
                                                                   
                 - The program execution can be stopped anytime by 
                   pressing Control-C                              
                 - Leave the contours/edges of the array blank.    
                                          
