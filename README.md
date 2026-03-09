# 🐚 Custom POSIX-Compliant Shell

![C](https://img.shields.io/badge/Language-C-00599C?style=for-the-badge&logo=c)
![POSIX](https://img.shields.io/badge/Compliant-POSIX-success?style=for-the-badge)
![CodeCrafters](https://img.shields.io/badge/Challenge-CodeCrafters-F2A541?style=for-the-badge)

This repository contains my solution to the **["Build Your Own Shell" Challenge](https://app.codecrafters.io/courses/shell/overview)** on CodeCrafters.

I built a custom, fully interactive shell from the ground up using **C**. This project covers everything from basic command execution to advanced terminal manipulation, file descriptor routing, and process orchestration.

---

## ✨ Key Features

### ⚙️ 1. Command Execution & Builtins
The shell natively handles both internal commands and external system binaries seamlessly.
* **Builtins Supported:** `echo`, `exit`, `pwd`, `cd`, `type`, and `history`.
* **External Commands:** Utilizes the standard Unix process API (`fork()`, `execvp()`, and `waitpid()`) to execute any program available in the system's `$PATH`.

### ✂️ 2. Advanced Input Parsing
A custom lexer/parser ensures user input is handled exactly like a standard Unix shell:
* **Single Quotes (`' '`):** Preserves the literal value of all enclosed characters.
* **Double Quotes (`" "`):** Preserves text while allowing for specific escape sequences.
* **Backslashes (`\`):** Robust handling of escaped spaces and special characters.

### ⌨️ 3. TAB Autocompletion (Interactive Engine)
By transitioning the terminal into **Raw Mode** via `<termios.h>`, the shell intercepts keystrokes in real-time:
* **Dynamic Completion:** Searches internal builtins and all executables in the `$PATH`.
* **Longest Common Prefix (LCP):** Automatically completes the shared prefix of multiple matches. If matches diverge, it rings the terminal bell (`\x07`).
* **Path & Filename Support:** Context-aware completion for nested directories (e.g., `cd path/to/d`<kbd>TAB</kbd>).
* **Smart Formatting:** Appends a `/` for directories and a ` ` (space) for files. Pressing <kbd>TAB</kbd> twice lists all available options cleanly.

### 🔀 4. Pipelines & Redirection
* **Pipelines (`|`):** Implements a robust pipeline engine capable of chaining multiple commands. It dynamically manages `pipe()` ends across multiple child processes to prevent hanging or memory leaks.
* **I/O Redirection:** * Standard Output: Overwrite (`>`) and Append (`>>`).
  * Standard Error: Overwrite (`2>`) and Append (`2>>`).

### 📜 5. History Management
* **Session Persistence:** Commands are loaded into memory on startup and saved safely to disk upon exiting.
* **Interactive Navigation:** Use the <kbd>↑</kbd> and <kbd>↓</kbd> arrow keys to cycle through previously executed commands.
* **Environment Aware:** Dynamically resolves the `HISTFILE` environment variable, falling back to `~/.shell_history` if unset.
* **History Flags:** Full support for `history -r` (read), `history -w` (write), and `history -a` (append session).

---

## 📂 Project Structure

```text
📦 codecrafters-shell-c
 ┣ 📂 src
 ┃ ┗ 📜 main.c          # Core REPL loop, parser, and execution engine
 ┣ 📜 your_program.sh   # Entry point script
 ┗ 📜 README.md         # Project documentation
 ```

---

## 🚀 How to Run Locally

To test and use this shell on your local machine, follow these steps:

**1. Clone the repository:**
```bash
git clone [https://github.com/numele-tau/numele-repo-ului.git](https://github.com/numele-tau/numele-repo-ului.git)
cd numele-repo-ului
```
[![progress-banner](https://backend.codecrafters.io/progress/shell/78a22f06-c5a6-4502-ade0-1a5d270da6d8)](https://app.codecrafters.io/users/codecrafters-bot?r=2qF)
