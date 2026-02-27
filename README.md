#  MyShell – A Custom Linux Shell in C++

A lightweight custom shell built from scratch in C++ that mimics core features of Linux terminals. MyShell lets users execute both built-in and external commands, manage background jobs, and explore system-level concepts.

---

## Features

- ✅ Built-in commands: `cd`, `pwd`, `exit`, `history`
- ✅ External commands using `execvp`
- ✅ Background process execution (`&`)
- ✅ Custom shell prompt with current working directory
---

## Demo (Sample Output)

```bash
[MyShell:/home/lokesh]$ pwd
/home/lokesh

[MyShell:/home/lokesh]$ cd ..
[MyShell:/home]$ sleep 3 &
[Background PID: 3242]

[MyShell:/home]$ history
1  pwd
2  cd ..
3  sleep 3 &
4  history

---

##  Build & Run

```bash
g++ main.cpp -o myshell
./myshell
