#include <string>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <signal.h>
#include <fstream>

using namespace std;
int my_cd(vector<char *> args)
{
    chdir(args[1]);
    exit(0);
}

int my_ls(vector<char *> args)
{
    const size_t size = 1024;
    char directory[size];
    getcwd(directory, size);

    for (auto &entry : filesystem::directory_iterator(directory))
    {
        cout << entry.path().filename().string() << " ";
    }
    cout << endl;
    exit(0);
}

int my_pwd(vector<char *> args)
{
    char buffer[1024];

    if (getcwd(buffer, sizeof(buffer)) == nullptr)
    {
        perror("getcwd");  // Print error like execvp
        exit(1);  // Exit on failure, like execvp would do
    }

    write(STDOUT_FILENO, buffer, strlen(buffer));  // Use write() like execvp would
    write(STDOUT_FILENO, "\n", 1);  // Ensure newline, since execvp would flush output
    exit(0);  // Exit after execution, as execvp replaces the process
}


int my_history(vector<char *> args)
{
    vector<char *> cmd = {
        const_cast<char *>("cat"), const_cast<char *>(".ccsh_history"), nullptr};
    execvp(cmd[0], cmd.data());
    perror("history failed");
    return 1;
}

char *builtinfunc_list[] = {"cd", "ls", "pwd", "history"};
std::vector<int (*)(vector<char *>)> builtin_func = {my_cd, my_ls, my_pwd, my_history};

void displayPrompt()
{
    cout << "ccsh > ";
}

void sigintHandler(int sig)
{
    cout << endl;
    cout.flush();
}

vector<vector<string>> parseCommands(string input) {
    vector<vector<string>> commands;
    vector<string> tokens; 
    bool inQuotes = false;
    string currentToken;

    for (int i = 0; i < input.length(); i++) {
        if (input[i] != '|' && input[i] != ' ') {
            currentToken += input[i];
        }
        if (input[i] == '|') {
            commands.push_back(tokens);
            tokens.clear();
        }
        if (input[i] == ' ') {
            if (!currentToken.empty()) tokens.push_back(currentToken);
            currentToken.clear();
        }
    }
    if (!currentToken.empty()) tokens.push_back(currentToken);
    if (!tokens.empty()) commands.push_back(tokens);
    return commands;
}


int executeCommand(vector<string> command)
{
    vector<char *> c_args;

    for (const string &arg : command)
    {
        c_args.push_back(const_cast<char *>(arg.c_str()));
    }
    c_args.push_back(nullptr);

    for (size_t i = 0; i < builtin_func.size(); i++)
    {
        if (strcmp(c_args[0], builtinfunc_list[i]) == 0)
        {
            return builtin_func[i](c_args);
        }
    }

    execvp(c_args[0], c_args.data());

    perror("execute command failed");
    return 1;
}

void closeAllPipes(vector<int *> pipes, int n)
{
    for (int i = 0; i < n; i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

void saveToHistory(string input)
{
    ofstream history(".ccsh_history", ios_base::app);
    history << input << endl;
    history.close();
}

void executeCommandLoop(vector<vector<string>> commands)
{
    if (commands.size() == 1 && commands[0].size() == 1 && commands[0][0] == "exit")
        exit(0);
    int command_index = 0;
    vector<int *> pipes(commands.size());

    for (int i = 0; i < commands.size(); i++)
    {
        pipes[i] = new int[2];
    }
    for (int pipe_index = 0; pipe_index < commands.size(); pipe_index++)
    {
        if (pipe(pipes[pipe_index]) == -1) {
            perror("pipe failed");
            exit(1);
        }
    }

    vector<pid_t> pids(commands.size());
    for (command_index = 0; command_index < commands.size(); command_index++)
    {
        if (commands[command_index].empty())
            return;

        pid_t pid = fork();

        if (pid == 0)
        {
            struct sigaction sa_default;
            sa_default.sa_handler = SIG_DFL;
            sa_default.sa_flags = 0;
            sigemptyset(&sa_default.sa_mask);
            sigaction(SIGINT, &sa_default, NULL);

            if (command_index > 0)
            {
                dup2(pipes[command_index - 1][0], STDIN_FILENO);
            }

            if (command_index + 1 < commands.size())
            {
                dup2(pipes[command_index][1], STDOUT_FILENO);
            }

            closeAllPipes(pipes, commands.size());
            if (executeCommand(commands[command_index]))
            {
                perror("execute command loop failed");
                exit(1);
            }
        }
        else
        {
            pids[command_index] = pid;

    close(pipes[command_index][1]); 
    if (command_index > 0) {
        close(pipes[command_index - 1][0]);
    }
        }
    }
    closeAllPipes(pipes, commands.size());
    int i = 0;
    for (i = 0; i < commands.size(); i++)
    {
        waitpid(pids[i], NULL, WUNTRACED);
    }
}

int main()
{

    struct sigaction sa_restart;
    sa_restart.sa_handler = sigintHandler;
    sigemptyset(&sa_restart.sa_mask);
    sa_restart.sa_flags = SA_RESTART;

    sigaction(SIGINT, &sa_restart, NULL);
    while (true)
    {
        string input = "";
        displayPrompt();

        getline(cin, input);
        saveToHistory(input);
        if (cin.eof())
            break;

        vector<vector<string>> commands = parseCommands(input);

        executeCommandLoop(commands);
    }

    cout << "Exiting... ";
    return 0;
}