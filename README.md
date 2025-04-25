# w25shell 🐚

A lightweight Unix-like shell implementation in C that supports advanced piping, I/O redirection, conditional command execution, and custom file operations.

## Features ✨

- **Command Execution** 💻: Execute standard Unix commands
- **I/O Redirection** 📤📥: Support for `<`, `>`, and `>>` operators
- **Piping** 🔄: Standard (`|`) and reverse (`=`) piping between commands (up to 5 pipes)
- **Conditional Execution** ⚙️: Support for `&&` and `||` operators
- **Sequential Execution** ⏩: Run multiple commands with `;` separator
- **Special File Operations** 📂:
  - Word counting: `# filename`
  - File content swapping: `file1 ~ file2`
  - File concatenation: `file1 + file2 + file3`
- **Process Management** 🔄:
  - `killterm`: Terminate current shell instance
  - `killallterms`: Terminate all instances of the shell

## Building and Running 🛠️

```bash
# Compile the code
gcc -o w25shell w25shell.c

# Run the shell
./w25shell
```

## Usage Examples 📝

```bash
# Basic command execution
w25shell$ ls -la

# I/O redirection
w25shell$ ls > filelist.txt
w25shell$ sort < input.txt > sorted.txt

# Piping
w25shell$ ls -la | grep .txt | wc -l

# Reverse piping (output of right commands flows to input of left commands)
w25shell$ wc -l = grep .txt = ls -la

# Conditional execution
w25shell$ mkdir test && cd test || echo "Failed to create directory"

# Sequential execution
w25shell$ date ; uptime ; who

# File operations
w25shell$ # textfile.txt             # Count words in textfile.txt
w25shell$ file1.txt ~ file2.txt      # Swap contents of files
w25shell$ file1.txt + file2.txt      # Concatenate and output files
```

## Limitations ⚠️

- Maximum command length: 1024 characters
- Maximum arguments per command: 5
- Maximum number of pipes: 5
- Maximum number of commands in a sequence: 5

## Error Handling 🐛

The shell includes robust error handling for:
- Invalid argument counts
- File operation failures
- Process creation errors
- Command execution errors
