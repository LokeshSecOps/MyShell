#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <map>

enum JobState { Running, Stopped };

struct Job {
    pid_t pid;
    std::string command;
    JobState state;
};

std::vector<std::string> history;
std::map<pid_t, Job> jobs;
pid_t foregroundPid = 0;

void handle_SIGTSTP(int sig) {
    if (foregroundPid > 0) {
        kill(foregroundPid, SIGTSTP);
    }
}

void handle_SIGCHLD(int sig) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        jobs.erase(pid);
    }
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> parts;
    std::string token;
    std::istringstream ss(str);
    while (std::getline(ss, token, delimiter)) {
        if (!token.empty()) parts.push_back(token);
    }
    return parts;
}

void execute_piped_commands(const std::vector<std::string>& commands, bool runInBackground, const std::string& fullCmd) {
    int n = commands.size();
    int pipefd[2], in_fd = 0;

    pid_t lastPid = -1;
    for (int i = 0; i < n; ++i) {
        pipe(pipefd);

        std::string inputFile, outputFile;
        bool append = false;
        std::vector<char*> args;

        std::istringstream iss(commands[i]);
        std::string token;
        while (iss >> token) {
            if (token == "<") {
                iss >> inputFile;
            } else if (token == ">") {
                iss >> outputFile;
                append = false;
            } else if (token == ">>") {
                iss >> outputFile;
                append = true;
            } else {
                args.push_back(strdup(token.c_str()));
            }
        }
        args.push_back(nullptr);

        pid_t pid = fork();
        if (pid == 0) {
            setpgid(0, 0);

            if (i > 0) {
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }

            if (i < n - 1) {
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }

            if (!inputFile.empty()) {
                int fd = open(inputFile.c_str(), O_RDONLY);
                if (fd < 0) {
                    perror("Input file error");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            if (!outputFile.empty() && i == n - 1) {
                int fd = open(outputFile.c_str(), O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 0644);
                if (fd < 0) {
                    perror("Output file error");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            close(pipefd[0]);
            execvp(args[0], args.data());
            perror("Command failed");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            setpgid(pid, pid);
            if (i == n - 1) lastPid = pid;
        } else {
            perror("fork failed");
        }

        for (char* arg : args) free(arg);

        close(pipefd[1]);
        if (i > 0) close(in_fd);
        in_fd = pipefd[0];
    }

    if (runInBackground) {
        jobs[lastPid] = {lastPid, fullCmd, Running};
        std::cout << "[Background PID: " << lastPid << "]" << std::endl;
    } else {
        foregroundPid = lastPid;
        waitpid(lastPid, nullptr, 0);
        foregroundPid = 0;
    }
}

int main() {
    signal(SIGTSTP, handle_SIGTSTP);
    signal(SIGCHLD, handle_SIGCHLD);

    while (true) {
        char cwd[PATH_MAX];
        getcwd(cwd, sizeof(cwd));
        std::cout << "[MyShell:" << cwd << "]$ ";
        std::cout.flush();

        std::string input;
        std::getline(std::cin, input);
        if (input.empty()) continue;
        history.push_back(input);

        bool runInBackground = false;
        input.erase(input.find_last_not_of(" \t\n\r\f\v") + 1);
        if (!input.empty() && input.back() == '&') {
            runInBackground = true;
            input.pop_back();
            input.erase(input.find_last_not_of(" \t") + 1);
        }

        if (input == "exit") break;

        if (input == "pwd") {
            std::cout << cwd << std::endl;
            continue;
        }

        if (input == "history") {
            for (size_t i = 0; i < history.size(); ++i)
                std::cout << i + 1 << "  " << history[i] << std::endl;
            continue;
        }

        if (input == "jobs") {
            for (const auto& [pid, job] : jobs) {
                std::cout << "[" << pid << "] " << (job.state == Running ? "Running" : "Stopped")
                          << " - " << job.command << std::endl;
            }
            continue;
        }

        if (input == "fg") {
            bool found = false;
            for (auto& [pid, job] : jobs) {
                if (job.state == Stopped || job.state == Running) {
                    foregroundPid = pid;
                    job.state = Running;
                    kill(pid, SIGCONT);
                    waitpid(pid, nullptr, 0);
                    jobs.erase(pid);
                    found = true;
                    break;
                }
            }
            if (!found) std::cout << "No stopped/running jobs to bring to foreground.\n";
            continue;
        }

        if (input == "bg") {
            bool found = false;
            for (auto& [pid, job] : jobs) {
                if (job.state == Stopped) {
                    job.state = Running;
                    kill(pid, SIGCONT);
                    std::cout << "[Resumed PID: " << pid << "]" << std::endl;
                    found = true;
                    break;
                }
            }
            if (!found) std::cout << "No stopped jobs to resume in background.\n";
            continue;
        }

        if (input.substr(0, 2) == "cd") {
            std::istringstream iss(input);
            std::string cmd, path;
            iss >> cmd >> path;
            const char* targetDir = path.empty() ? getenv("HOME") : path.c_str();
            if (chdir(targetDir) != 0) perror("cd failed");
            continue;
        }

        // Handle pipes
        std::vector<std::string> commands = split(input, '|');
        execute_piped_commands(commands, runInBackground, input);
    }

    return 0;
}
