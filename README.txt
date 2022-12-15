Author:	Serkan Bayramoglu
Email:	bayramoglu.serkan@gmail.com
Date:		July 24, 2022
Class:	CS344
Assignment:3
File name:	smallsh.c


Subject:	A simple shell program built in C running on linux system, that performs 3 built in functions, 
		and runs the rest of the commands via execvp() function.


Compilation:

		Once saved under a folder in Linux system the file needs to be compiled with either;
	
							gcc -std=c99 -o smallsh smallsh.c

		or,

							gcc -std=gnu99 -o smallsh smallsh.c

		where smallsh.c is the name of the file that has the code, and smallsh is the name of the 
		executable file that will be created with the compilation.

		After compilation, smallsh can be entered in the command line to run the shell.    


Instructions:	
			
		Command line: 
				
		The shell accepts input in the following format, the square brackets are optional. The  
		shell supports command lines with up to 2048 characters, where spaces are also counted as 
		characters, consisting of up to 512 arguments, where the command and each of the < > and 
		& characters (each acting like an operator) are also considered as an argument. The shell 
		does not support quoting, therefore any argument must not have a space inside. The command
		line format is as follows: 


				command [arg1 arg2 ...] [< input_file] [> output_file] [&]



		command 		This can be any linux command, or one of the built in functions 
					
		[arg1, ...] 	If entered, these are the arguments to be used with the command

		[< input_file]	If entered, this is the input redirection used with the command
			
		[> output_file] If entered, this is the output redirection used with the command

		[&]			If entered, (under normal mode) the command runs on the background

		Notes: 		There must be at least one space between each arguments, otherwise it 
					is considered as one argument together with the other characters preceded 
					and followed by the character 
							
					if & character is not placed as the last character after a space, it is 
					considered as an ordinary character with no special function

					the input_file and output_file names have to be be followed by the 
					redirection characters separated with space

					the input and output redirections have to placed at the end, but before 
					& character, if any, and either of them can be before or after the other
			
					the built in functions explained below do not work on the background, 
					even if & character is used 

					the shell can support up to 100 background processes at a time, a message
					is displays when reached to the limit asking to wait until a background 
					process ends before a new background process can be started 

					empty lines are disregarded, when entered, the shell re-prompts for new 
					input, without taking any further action

					lines which start with # character are considered comment lines, therefore
					any arguments in the line starting with # are not taken into consideration,
					the shell will re-prompt without taking any action

					if two consecutive characters, $$ are entered together in any argument, or 
					as an independent argument, each of the double dollar character is replaced 
					with the process id of the parent process, which is the shell 
 

		Built in functions:

		smallsh has 3 built in functions; cd, status and exit: 
					
		exit        	exits the shell 

					does not take any argument, any argument after command is disregarded

					any active foreground and background child processes are terminated together 
					with the parent

		cd []			changes the working directory to the path provided in the brackets 

					if there is no path in the brackets, the working directory changes to the HOME 
					folder provided in the environment variables
			
		status		returns the exit / termination and signal status of the last most recent child 
					process, which stopped due to either because the process ended as expected or 
					unexpectedly due to a problem, or terminated externally 

					does not take any argument, any argument following the command is disregarded

		Notes:		the built commands only work on the foreground, id an & character is entered at  
					the end, it is disregarded

					The built in functions cannot be redirected, any redirection entered towards the 
					end is disregarded


		Special Signals:	
	
		The usage of the two special signals are controlled by th shell as follows:

		SIGINT (CTRL-C)	When pressed / entered in the command line, the parent and any child processes running
					on the background ignore this signal, and is not affected by this signal.

					A child process running on the foreground terminates its process.

					The shell as the parent prints out the process id number of the terminated child together 
					with its signal on stdout (on the terminal), before prompting the user for a new input
		

		SIGTSTP (CTRL-Z)	All children processing on the foreground and background ignore the signal and do not
					take any action.
					
					The shell, as the parent switches the mode of the shell to foreground-only mode, the 
					shell is at the time working on the normal mode, or exits the foreground only mode if 
					the shell at time is in foreground only mode

					when on the foreground only mode, the shell does not accept any background processes, 
					all the entered new processes, even if & is used in the end are run on the foreground

					while switching the shell prints, to which mode it has switched

		Background Processes:
		
		The can either process the jobs on the foreground, or on the background. 

		Only one foreground process can run at a time, and the user does not have any control until the end of the 
		foreground process, unless he/she terminates the process by pressing CTRL-C to send the SIGINT signal.

		The shell supports processing of up to 100 child processes on the background at a given time. While the
		background processes work, the shell can be used by the user. When a background process starts, the shell 
		prints the process id os the background process that started. As soon as a process terminates, with the
		SIGCHLD signal produced, before prompting for a new input, the shell prints; the process id of the ended 
		process, whether the process is exited or terminated, and the signal number of the exit or termination 
		process.
		
