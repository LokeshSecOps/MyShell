#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include <cstdlib>
#include <cstring>

int main() {
    std::vector<std::string> history;

    while (true) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            std::cout << "[MyShell:" << cwd << "]$ ";
        } else {
            std::cout << "[MyShell:unknown]$ ";
        }

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

        if (input == "exit") {
            break;
        }

        if (input == "pwd") {
            if (getcwd(cwd, sizeof(cwd)) != nullptr) {
                std::cout << cwd << std::endl;
            } else {
                perror("getcwd failed");
            }
            continue;
        }

        if (input.substr(0, 2) == "cd") {
            std::istringstream iss(input);
            std::string cmd, path;
            iss >> cmd >> path;

            const char* targetDir;
            if (path.empty()) {
                targetDir = getenv("HOME");
            } else {
                targetDir = path.c_str();
            }

            if (chdir(targetDir) != 0) {
                perror("cd failed");
            }
            continue;
        }

        if (input == "history") {
            for (size_t i = 0; i < history.size(); ++i) {
                std::cout << i + 1 << "  " << history[i] << std::endl;
            }
            continue;
        }

        std::istringstream iss(input);
        std::vector<char*> args;
        std::string token;
        while (iss >> token) {
            args.push_back(strdup(token.c_str())); 
        }
        args.push_back(nullptr);

        pid_t pid = fork();
        if (pid == 0) {
            
            execvp(args[0], args.data());
            perror("Command failed");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            
            if (runInBackground) {
                std::cout << "[Background PID: " << pid << "]" << std::endl;
            } else {
                waitpid(pid, nullptr, 0); 
            }
        } else {
            perror("fork failed");
        }

        for (char* arg : args) {
            free(arg);
        }
    }

    return 0;
}
