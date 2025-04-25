#include <stdio.h>              
#include <stdlib.h>             
#include <string.h>             
#include <unistd.h>             
#include <sys/types.h>          
#include <sys/wait.h>           
#include <fcntl.h>              
#include <signal.h>             
#include <dirent.h>             
#include <ctype.h>              
#include <errno.h>              

#define MAX_CMD_LEN 1024        
#define MAX_ARGS 5              
#define MAX_PIPES 5             
#define MAX_COMMANDS 5          

// Process Management Functions
int validate_argc(int argc) {   // This function checks if the number of arguments is okay
    return (argc >= 1 && argc <= MAX_ARGS);  // Returns true if args are between 1 and MAX_ARGS
}

void killterm() {               // Simple function to terminate the current shell
    exit(0);                    // Just exits with status 0 (success)
}

void killallterms() {           // This function kills all instances of our shell
    pid_t my_pid = getpid();    // Gets our current process ID
    char self_name[256] = {0};  // Array to store our process name, initialized to zeros
    FILE *fp;                   // File pointer for reading process info
    char line[256];             // Buffer for reading lines from process list
    int found = 0, killed = 0;  // Counters for processes found and killed

    // Get process name
    FILE *cmd_fp = fopen("/proc/self/cmdline", "r");  // Opens our command line info
    if (cmd_fp) {               // If we successfully opened the file
        size_t bytes = fread(self_name, 1, sizeof(self_name) - 1, cmd_fp);  // Read our command name
        fclose(cmd_fp);         // Close the file
        self_name[bytes] = '\0'; // Null terminate the string
        char *basename = strrchr(self_name, '/');  // Find the last slash
        if (basename) memmove(self_name, basename + 1, strlen(basename));  // Keep just the filename
        for (int i = 0; i < sizeof(self_name) - 1 && self_name[i]; i++) {  // Clean up any junk
            if (self_name[i] == '\0') {  // If we hit a null character
                self_name[i] = '\0';     // Make sure it's properly terminated
                break;                   // Stop the loop
            }
        }
    } else {                    // If we couldn't open the file
        perror("Failed to get process name");  // Print an error
        strcpy(self_name, "w25shell");  // Default to "w25shell"
    }

    // Find and kill processes
    char command[256];          // Buffer for our ps command
    snprintf(command, sizeof(command), "ps -u %d -o pid,comm", getuid());  // Build command to list processes
    fp = popen(command, "r");   // Run the command and open a pipe to read output
    if (!fp) {                  // If the pipe failed
        perror("popen failed"); // Print an error
        return;                 // Give up
    }

    while (fgets(line, sizeof(line), fp) != NULL) {  // Read each line of process info
        int proc_id;            // To store process ID
        char proc_name[256];    // To store process name
        if (sscanf(line, "%d %s", &proc_id, proc_name) == 2) {  // Parse PID and name
            if (strcmp(proc_name, self_name) == 0) {  // If this is our shell
                found++;        // Increment found counter
                if (proc_id != my_pid) {  // If it's not our current process
                    printf("Attempting to kill %s PID %d\n", self_name, proc_id);  // Announce attempt
                    if (kill(proc_id, SIGTERM) == -1) {  // Try to terminate nicely
                        fprintf(stderr, "Failed to kill PID %d: %s\n", proc_id, strerror(errno));  // Report failure
                    } else {        // If SIGTERM worked
                        usleep(100000);  // Wait a bit (100ms)
                        if (kill(proc_id, 0) == -1 && errno == ESRCH) {  // Check if process is gone
                            printf("Successfully killed PID %d with SIGTERM\n", proc_id);  // Success!
                            killed++;  // Increment killed counter
                        } else if (kill(proc_id, SIGKILL) == -1) {  // Try forceful kill
                            fprintf(stderr, "Failed to kill PID %d with SIGKILL: %s\n", proc_id, strerror(errno));  // Report failure
                        } else {    // If SIGKILL worked
                            printf("Successfully killed PID %d with SIGKILL\n", proc_id);  // Success!
                            killed++;  // Increment killed counter
                        }
                    }
                }
            }
        }
    }
    pclose(fp);                 // Close the pipe

    // Print results and kill self
    if (found == 0) printf("No %s instances found\n", self_name);  // No instances at all
    else if (found == 1) printf("No other %s instances found\n", self_name);  // Only us
    else printf("Found %d %s instances, killed %d\n", found - 1, self_name, killed);  // Report results

    if (found > 0) {            // If we found any instances
        printf("Killing self: %s PID %d\n", self_name, my_pid);  // Announce self-destruction
        fflush(stdout);         // Make sure output is shown
        usleep(100000);         // Wait a bit (100ms)
        kill(my_pid, SIGKILL);  // Kill ourselves forcefully
    }
}

// I/O Redirection
void handle_redirection(char **args) {  // Handles input/output redirection
    for (int i = 0; args[i]; i++) {     // Loop through all arguments
        if (strcmp(args[i], "<") == 0) {  // If we see input redirection
            if (!args[i + 1]) {         // Check if there's a filename after <
                fprintf(stderr, "Error: < requires a file argument\n");  // Complain if not
                return;                 // Stop processing
            }
            int fd = open(args[i + 1], O_RDONLY);  // Open the file for reading
            if (fd < 0) {           // If opening failed
                perror("Error opening file for input");  // Report the error
                return;             // Stop processing
            }
            dup2(fd, STDIN_FILENO);  // Redirect stdin to this file
            close(fd);              // Close the file descriptor
            args[i] = NULL;         // Remove the < from args
        } else if (strcmp(args[i], ">") == 0) {  // If we see output redirection
            if (!args[i + 1]) {     // Check if there's a filename after >
                fprintf(stderr, "Error: > requires a file argument\n");  // Complain if not
                return;             // Stop processing
            }
            int fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);  // Open file for writing
            if (fd < 0) {           // If opening failed
                perror("Error opening file for output");  // Report the error
                return;             // Stop processing
            }
            dup2(fd, STDOUT_FILENO);  // Redirect stdout to this file
            close(fd);              // Close the file descriptor
            args[i] = NULL;         // Remove the > from args
        } else if (strcmp(args[i], ">>") == 0) {  // If we see append redirection
            if (!args[i + 1]) {     // Check if there's a filename after >>
                fprintf(stderr, "Error: >> requires a file argument\n");  // Complain if not
                return;             // Stop processing
            }
            int fd = open(args[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);  // Open file for appending
            if (fd < 0) {           // If opening failed
                perror("Error opening file for appending");  // Report the error
                return;             // Stop processing
            }
            dup2(fd, STDOUT_FILENO);  // Redirect stdout to append to this file
            close(fd);              // Close the file descriptor
            args[i] = NULL;         // Remove the >> from args
        }
    }
}

// Command Execution Functions
void execute_command(char **args) {  // Runs a single command
    if (!args[0]) return;           // If no command, just return
    int argc = 0;                   // Counter for arguments
    while (args[argc]) argc++;      // Count how many arguments we have
    if (!validate_argc(argc)) {     // Check if argument count is valid
        fprintf(stderr, "Argument count must be between 1 and %d\n", MAX_ARGS);  // Complain if not
        return;                     // Stop processing
    }

    pid_t pid = fork();             // Create a new process
    if (pid == 0) {                 // In the child process
        handle_redirection(args);   // Set up any I/O redirection
        execvp(args[0], args);      // Execute the command
        perror("exec failed");      // If we get here, exec failed
        exit(1);                    // Exit child with error
    } else if (pid > 0) {           // In the parent process
        wait(NULL);                 // Wait for child to finish
    } else {                        // If fork failed
        perror("fork failed");      // Report the error
    }
}

void execute_piped_commands(char ***commands, int num_pipes) {  // Handles piped commands
    if (num_pipes >= MAX_PIPES) {  // Check if we have too many pipes
        fprintf(stderr, "Maximum %d pipes allowed\n", MAX_PIPES - 1);  // Complain if so
        return;                    // Stop processing
    }

    int pipefd[2 * num_pipes];     // Array for pipe file descriptors
    for (int i = 0; i < num_pipes; i++) {  // Create all needed pipes
        if (pipe(pipefd + i * 2) < 0) {  // Try to create a pipe
            perror("pipe failed"); // Report if it fails
            return;                // Stop processing
        }
    }

    pid_t pid;                     // Process ID variable
    for (int i = 0; i <= num_pipes; i++) {  // For each command in the pipeline
        int argc = 0;              // Counter for arguments
        while (commands[i][argc]) argc++;  // Count arguments for this command
        if (!validate_argc(argc)) {  // Check if argument count is valid
            fprintf(stderr, "Command %d: Argument count must be between 1 and %d\n", i + 1, MAX_ARGS);  // Complain if not
            return;                // Stop processing
        }

        pid = fork();              // Create a new process
        if (pid == 0) {            // In the child process
            if (i > 0) dup2(pipefd[(i - 1) * 2], STDIN_FILENO);  // Connect input from previous pipe
            if (i < num_pipes) dup2(pipefd[i * 2 + 1], STDOUT_FILENO);  // Connect output to next pipe
            for (int j = 0; j < 2 * num_pipes; j++) close(pipefd[j]);  // Close all pipe ends
            handle_redirection(commands[i]);  // Set up any additional redirection
            execvp(commands[i][0], commands[i]);  // Execute the command
            perror("execvp failed");  // If we get here, exec failed
            exit(1);                  // Exit child with error
        } else if (pid < 0) {         // If fork failed
            perror("fork failed");    // Report the error
            return;                   // Stop processing
        }
    }

    for (int i = 0; i < 2 * num_pipes; i++) close(pipefd[i]);  // Parent closes all pipe ends
    for (int i = 0; i <= num_pipes; i++) wait(NULL);  // Wait for all children to finish
}

void execute_reverse_piped_commands(char ***commands, int num_pipes) {  // Handles reverse piped commands
    if (num_pipes >= MAX_PIPES) {  // Check if we have too many pipes
        fprintf(stderr, "Maximum %d reverse pipes allowed\n", MAX_PIPES - 1);  // Complain if so
        return;                    // Stop processing
    }

    int pipefd[2 * num_pipes];     // Array for pipe file descriptors
    for (int i = 0; i < num_pipes; i++) {  // Create all needed pipes
        if (pipe(pipefd + i * 2) < 0) {  // Try to create a pipe
            perror("pipe failed"); // Report if it fails
            return;                // Stop processing
        }
    }

    pid_t pid;                     // Process ID variable
    for (int i = num_pipes; i >= 0; i--) {  // For each command, in reverse order
        int argc = 0;              // Counter for arguments
        while (commands[i][argc]) argc++;  // Count arguments for this command
        if (!validate_argc(argc)) {  // Check if argument count is valid
            fprintf(stderr, "Command %d: Argument count must be between 1 and %d\n", num_pipes - i + 1, MAX_ARGS);  // Complain if not
            return;                // Stop processing
        }

        pid = fork();              // Create a new process
        if (pid == 0) {            // In the child process
            if (i < num_pipes) dup2(pipefd[i * 2], STDIN_FILENO);  // Connect input from next pipe
            if (i > 0) dup2(pipefd[(i - 1) * 2 + 1], STDOUT_FILENO);  // Connect output to previous pipe
            for (int j = 0; j < 2 * num_pipes; j++) close(pipefd[j]);  // Close all pipe ends
            handle_redirection(commands[i]);  // Set up any additional redirection
            execvp(commands[i][0], commands[i]);  // Execute the command
            perror("execvp failed");  // If we get here, exec failed
            exit(1);                  // Exit child with error
        } else if (pid < 0) {         // If fork failed
            perror("fork failed");    // Report the error
            return;                   // Stop processing
        }
    }

    for (int i = 0; i < 2 * num_pipes; i++) close(pipefd[i]);  // Parent closes all pipe ends
    for (int i = 0; i <= num_pipes; i++) wait(NULL);  // Wait for all children to finish
}

void execute_sequential_commands(char ***commands, int num_commands) {  // Runs commands one after another
    if (num_commands > 4) {     // Check if we have too many commands
        fprintf(stderr, "Maximum 4 sequential commands allowed\n");  // Complain if so
        return;                 // Stop processing
    }

    for (int i = 0; i < num_commands; i++) {  // For each command
        int argc = 0;           // Counter for arguments
        while (commands[i][argc]) argc++;  // Count arguments
        if (!validate_argc(argc)) {  // Check if argument count is valid
            fprintf(stderr, "Command %d: Argument count must be between 1 and %d\n", i + 1, MAX_ARGS);  // Complain if not
            continue;           // Skip this command
        }
        execute_command(commands[i]);  // Run the command
    }
}

void execute_conditional_commands(char ***commands, int num_commands, char **operators) {  // Handles && and || commands
    if (num_commands > MAX_COMMANDS) {  // Check if we have too many commands
        fprintf(stderr, "Maximum %d conditional commands allowed\n", MAX_COMMANDS);  // Complain if so
        return;                 // Stop processing
    }

    int status;                 // To store command exit status
    int prev_success = 1;       // Tracks if previous command succeeded

    for (int i = 0; i < num_commands; i++) {  // For each command
        int argc = 0;           // Counter for arguments
        while (commands[i][argc]) argc++;  // Count arguments
        if (!validate_argc(argc)) {  // Check if argument count is valid
            fprintf(stderr, "Command %d: Argument count must be between 1 and %d\n", i + 1, MAX_ARGS);  // Complain if not
            return;             // Stop processing
        }

        if (i == 0 || (prev_success && strcmp(operators[i - 1], "&&") == 0) ||  // Should we run this command?
            (!prev_success && strcmp(operators[i - 1], "||") == 0)) {
            pid_t pid = fork();  // Create a new process
            if (pid == 0) {      // In the child process
                handle_redirection(commands[i]);  // Set up redirection
                execvp(commands[i][0], commands[i]);  // Execute the command
                perror("exec failed");  // If we get here, exec failed
                exit(1);            // Exit child with error
            } else if (pid > 0) {   // In the parent process
                wait(&status);      // Wait for child and get status
                prev_success = (WIFEXITED(status) && WEXITSTATUS(status) == 0);  // Did it succeed?
            } else {               // If fork failed
                perror("fork failed");  // Report the error
                return;             // Stop processing
            }
        } else {               // If we shouldn't run this command
            break;             // Stop the loop
        }
    }
}

// File Operations
void execute_file_operations(char **args) {  // Handles special file operations
    int argc = 0;            // Counter for arguments
    while (args[argc]) argc++;  // Count arguments

    if (strcmp(args[0], "#") == 0) {  // If we're counting words
        if (argc != 2) {     // Check if we have exactly one filename
            fprintf(stderr, "# requires exactly 1 file argument\n");  // Complain if not
            return;          // Stop processing
        }
        FILE *file = fopen(args[1], "r");  // Open the file
        if (!file) {         // If opening failed
            perror("fopen failed");  // Report the error
            return;          // Stop processing
        }
        int word_count = 0;  // Counter for words
        char word[256];      // Buffer for reading words
        while (fscanf(file, "%255s", word) == 1) word_count++;  // Count each word
        fclose(file);        // Close the file
        printf("%d\n", word_count);  // Print the word count
    } else if (argc >= 3 && strcmp(args[1], "~") == 0) {  // If we're swapping file contents
        if (argc != 3) {     // Check if we have exactly two filenames
            fprintf(stderr, "~ requires exactly 2 file arguments\n");  // Complain if not
            return;          // Stop processing
        }
        FILE *file1 = fopen(args[0], "a+");  // Open first file for reading and appending
        FILE *file2 = fopen(args[2], "a+");  // Open second file for reading and appending
        if (!file1 || !file2) {  // If either file failed to open
            perror("fopen failed");  // Report the error
            if (file1) fclose(file1);  // Close any opened file
            if (file2) fclose(file2);  // Close any opened file
            return;          // Stop processing
        }

        fseek(file1, 0, SEEK_END);  // Go to end of first file
        long size1 = ftell(file1);  // Get its size
        fseek(file1, 0, SEEK_SET);  // Back to start
        char *buf1 = malloc(size1 + 1);  // Allocate buffer for contents
        fread(buf1, 1, size1, file1);  // Read contents
        buf1[size1] = '\0';    // Null terminate

        fseek(file2, 0, SEEK_END);  // Go to end of second file
        long size2 = ftell(file2);  // Get its size
        fseek(file2, 0, SEEK_SET);  // Back to start
        char *buf2 = malloc(size2 + 1);  // Allocate buffer for contents
        fread(buf2, 1, size2, file2);  // Read contents
        buf2[size2] = '\0';    // Null terminate

        fprintf(file1, "%s", buf2);  // Write second file's contents to first
        fprintf(file2, "%s", buf1);  // Write first file's contents to second

        free(buf1);            // Free the first buffer
        free(buf2);            // Free the second buffer
        fclose(file1);         // Close first file
        fclose(file2);         // Close second file
    } else if (argc >= 3 && strcmp(args[1], "+") == 0) {  // If we're concatenating files
        if (argc > MAX_ARGS + 1) {  // Check if we have too many files
            fprintf(stderr, "+ supports up to %d files\n", MAX_ARGS);  // Complain if so
            return;          // Stop processing
        }
        for (int i = 0; i < argc; i += 2) {  // For each file (skipping +)
            FILE *file = fopen(args[i], "r");  // Open the file
            if (!file) {     // If opening failed
                perror("fopen failed");  // Report the error
                continue;    // Skip to next file
            }
            char c;          // Character buffer
            while ((c = fgetc(file)) != EOF) putchar(c);  // Print each character
            fclose(file);    // Close the file
        }
    }
}

// Placeholder for builtin commands
void handle_builtin(char **args) {  // Placeholder for future built-in commands
    printf("Built-in commands not implemented\n");  // Just says it's not implemented
}

// Command Parser and Executor
void parse_and_execute(char *input) {  // Main function to parse and run commands
    char *commands[MAX_COMMANDS];  // Array for command strings
    char *operators[MAX_COMMANDS - 1];  // Array for operators between commands
    int num_commands = 0;      // Counter for number of commands

    // Parse commands and operators
    char *token = input;       // Start with the full input
    char *next_token;          // Pointer for next separator
    while (token && num_commands < MAX_COMMANDS) {  // While we have input and room
        next_token = strstr(token, ";");  // Look for sequential separator
        if (!next_token) next_token = strstr(token, "&&");  // Look for AND
        if (!next_token) next_token = strstr(token, "||");  // Look for OR

        if (next_token) {      // If we found a separator
            *next_token = '\0';  // Split the string here
            commands[num_commands++] = token;  // Store this command
            if (next_token[1] == '&') {  // If it's &&
                operators[num_commands - 1] = "&&";  // Store the operator
                token = next_token + 2;  // Move past &&
            } else if (next_token[1] == '|') {  // If it's ||
                operators[num_commands - 1] = "||";  // Store the operator
                token = next_token + 2;  // Move past ||
            } else {           // If it's just ;
                operators[num_commands - 1] = ";";  // Store the operator
                token = next_token + 1;  // Move past ;
            }
            while (*token == ' ') token++;  // Skip any spaces
        } else {               // If no more separators
            commands[num_commands++] = token;  // Store last command
            break;             // Done parsing
        }
    }

    // Check for conditional operators
    int has_conditional = 0;   // Flag for && or ||
    for (int i = 0; i < num_commands - 1; i++) {  // Check each operator
        if (strcmp(operators[i], "&&") == 0 || strcmp(operators[i], "||") == 0) {  // If conditional
            has_conditional = 1;  // Set the flag
            break;            // No need to check more
        }
    }

    if (has_conditional) {     // If we have && or ||
        char ***cmd_args = malloc(num_commands * sizeof(char **));  // Allocate array for args
        for (int i = 0; i < num_commands; i++) {  // For each command
            cmd_args[i] = malloc((MAX_ARGS + 1) * sizeof(char *));  // Allocate args array
            char *arg = strtok(commands[i], " ");  // Split by spaces
            int argc = 0;      // Counter for arguments
            while (arg && argc < MAX_ARGS) {  // While we have args and room
                cmd_args[i][argc++] = arg;  // Store the argument
                arg = strtok(NULL, " ");  // Get next argument
            }
            cmd_args[i][argc] = NULL;  // Null terminate the args array
        }
        execute_conditional_commands(cmd_args, num_commands, operators);  // Run conditional commands
        for (int i = 0; i < num_commands; i++) free(cmd_args[i]);  // Free each args array
        free(cmd_args);        // Free the main array
    } else if (num_commands > 1 && strcmp(operators[0], ";") == 0) {  // If sequential commands
        char ***cmd_args = malloc(num_commands * sizeof(char **));  // Allocate array for args
        for (int i = 0; i < num_commands; i++) {  // For each command
            cmd_args[i] = malloc((MAX_ARGS + 1) * sizeof(char *));  // Allocate args array
            char *arg = strtok(commands[i], " ");  // Split by spaces
            int argc = 0;      // Counter for arguments
            while (arg && argc < MAX_ARGS) {  // While we have args and room
                cmd_args[i][argc++] = arg;  // Store the argument
                arg = strtok(NULL, " ");  // Get next argument
            }
            cmd_args[i][argc] = NULL;  // Null terminate the args array
        }
        execute_sequential_commands(cmd_args, num_commands);  // Run sequential commands
        for (int i = 0; i < num_commands; i++) free(cmd_args[i]);  // Free each args array
        free(cmd_args);        // Free the main array
    } else {                   // Single command or piped commands
        // Handle piped commands
        char *pipe_commands[MAX_PIPES + 1];  // Array for piped command strings
        int num_pipes = 0;     // Counter for pipes
        int is_reverse = (strstr(input, "=") != NULL);  // Check for reverse pipe (using =)
        char *pipe_cmd = strtok(commands[0], "|=");  // Split by | or =
        while (pipe_cmd && num_pipes <= MAX_PIPES) {  // While we have commands and room
            pipe_commands[num_pipes++] = pipe_cmd;  // Store the command
            pipe_cmd = strtok(NULL, "|=");  // Get next command
        }

        if (num_pipes > 1) {   // If we have pipes
            char ***piped_args = malloc(num_pipes * sizeof(char **));  // Allocate array for args
            for (int j = 0; j < num_pipes; j++) {  // For each piped command
                piped_args[j] = malloc((MAX_ARGS + 1) * sizeof(char *));  // Allocate args array
                char *arg = strtok(pipe_commands[j], " ");  // Split by spaces
                int argc = 0;  // Counter for arguments
                while (arg && argc < MAX_ARGS) {  // While we have args and room
                    piped_args[j][argc++] = arg;  // Store the argument
                    arg = strtok(NULL, " ");  // Get next argument
                }
                piped_args[j][argc] = NULL;  // Null terminate the args array
            }
            if (is_reverse) execute_reverse_piped_commands(piped_args, num_pipes - 1);  // Run reverse pipes
            else execute_piped_commands(piped_args, num_pipes - 1);  // Run normal pipes
            for (int j = 0; j < num_pipes; j++) free(piped_args[j]);  // Free each args array
            free(piped_args);  // Free the main array
        } else {               // Single command
            char *args[MAX_ARGS + 1];  // Array for arguments
            int argc = 0;      // Counter for arguments
            char *arg = strtok(commands[0], " ");  // Split by spaces
            while (arg && argc < MAX_ARGS) {  // While we have args and room
                args[argc++] = arg;  // Store the argument
                arg = strtok(NULL, " ");  // Get next argument
            }
            args[argc] = NULL;  // Null terminate the args array

            if (argc > 0) {     // If we have a command
                if (strcmp(args[0], "#") == 0 || (argc > 1 && (strcmp(args[1], "~") == 0 || strcmp(args[1], "+") == 0))) {  // File operation?
                    execute_file_operations(args);  // Handle file operation
                } else if (strcmp(args[0], "killterm") == 0) {  // Kill current shell?
                    killterm();     // Terminate current shell
                } else if (strcmp(args[0], "killallterms") == 0) {  // Kill all shells?
                    killallterms();  // Terminate all shells
                } else {           // Regular command
                    execute_command(args);  // Run the command
                }
            }
        }
    }
}

// Main Shell Loop
int main() {                   // Main function where everything starts
    char input[MAX_CMD_LEN];   // Buffer for user input
    while (1) {                // Infinite loop for shell prompt
        printf("w25shell$ ");  // Show the prompt
        fflush(stdout);        // Make sure prompt appears immediately
        if (!fgets(input, MAX_CMD_LEN, stdin)) break;  // Read input, break on EOF
        input[strcspn(input, "\n")] = 0;  // Remove newline from input
        parse_and_execute(input);  // Process the command
    }
    return 0;                  // Exit with success (though we rarely get here)
}
