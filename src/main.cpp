#include <iostream>
#include <string>
#include <array>
#include <string_view>
#include <filesystem>
#include <ranges>
#include <system_error>
#include <vector>
#include <map>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>

#ifdef _WIN32
  constexpr char PATH_DELIMITER = ';';
#else
  constexpr char PATH_DELIMITER = ':';
#endif

enum CommandId
{
  Exit,
  Echo,
  Type,
  Pwd,
  Cd
};

enum StringState
{
  InsideDoubleQuotes,
  InsideSingleQuotes,
  Raw
};

bool CharIsQuote(const char& inChar) { return inChar == '\'' || inChar == '\"'; }

bool CharIsSimpleQuote(const char& inChar) { return inChar == '\''; }

bool CharIsDoubleQuote(const char& inChar) { return inChar == '\"'; }

bool CharIsEmpty(const char& inChar) { return inChar == ' '; }

bool CharIsBackSlash(const char& inChar) { return inChar == '\\'; }

const std::string GetEnvironmentVariable(const char* inName)
{
  const char* result = std::getenv(inName);
  if (result != nullptr)
  {
    return result;
  }
  return "";
}

const std::string GetHomeDirectory()
{
  std::string result("");

  #ifdef _WIN32
    result = GetEnvironmentVariable("USERPROFILE");
  #elif __linux__
    result = GetEnvironmentVariable("HOME");
  #endif

  return result;
}

bool PathExists(const std::filesystem::path& inPath)
{
  std::error_code error_code;
  std::filesystem::file_status path_status = std::filesystem::status(inPath, error_code);

  if (error_code.value() == 0 && std::filesystem::exists(path_status))
  {
    return true;
  }
  return false;
}

bool PathIsExecutable(const std::filesystem::path& inPath)
{
  std::error_code error_code;
  std::filesystem::file_status path_status = std::filesystem::status(inPath, error_code);
  std::filesystem::perms permissions = path_status.permissions();

  std::filesystem::perms exec_mask = std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec;

  if ((permissions & exec_mask) != std::filesystem::perms::none)
  {
    return true;
  }
  return false;
}

std::vector<std::string> ParseCommandParameters(std::string inParametersSection)
{
  std::vector<std::string> parameters;
  std::string current_param;
  StringState state = StringState::Raw;

  bool escape_next = false;
  bool has_content = false;

  for (size_t i = 0; i < inParametersSection.length(); ++i)
  {
    char c = inParametersSection[i];

    if (escape_next)
    {
      current_param += c;
      escape_next = false;
      has_content = true;
      continue;
    }

    if (c == '\\' && state != StringState::InsideSingleQuotes)
    {
      if (state == StringState::InsideDoubleQuotes)
      {
        if (i + 1 < inParametersSection.length() && (inParametersSection[i + 1] == '"' || inParametersSection[i + 1] == '\\' || inParametersSection[i + 1] == '$' || inParametersSection[i + 1] == '\n'))
          escape_next = true;
        else
        {
          current_param += c;
          has_content = true;
        }
      }
      else
      {
        escape_next = true;
      }
      continue;
    }

    if (c == '\'' && state != StringState::InsideDoubleQuotes)
    {
      state = (state == StringState::InsideSingleQuotes) ? StringState::Raw : StringState::InsideSingleQuotes;
      has_content = true;
      continue;
    }

    if (c == '"' && state != StringState::InsideSingleQuotes)
    {
      state = (state == StringState::InsideDoubleQuotes) ? StringState::Raw : StringState::InsideDoubleQuotes;
      has_content = true;
      continue;
    }

    if (c == ' ' && state == StringState::Raw)
    {
      if (has_content)
      {
        parameters.push_back(current_param);
        current_param.clear();
        has_content = false;
      }
      continue;
    }

    current_param += c;
    has_content = true;
  }

  if (has_content)
    parameters.push_back(current_param);

  return parameters;
}

const std::size_t GetCommandEndPos(const std::string& inUserInput)
{
  if (inUserInput.empty()) return 0;

  const char first_char = inUserInput.front();
  if (CharIsQuote(first_char))
  {
    for (size_t index = 1; index < inUserInput.length(); index++)
    {
      if (first_char == inUserInput[index])
        return index + 1;
    }
  }

  std::size_t command_end_pos = inUserInput.find(' ');

  if (command_end_pos != std::string::npos)
    return command_end_pos;

  return inUserInput.length();
}

const std::string GetCommandValue(const std::string& inUserInput, const size_t& inCommandEndPos)
{
  std::string raw_command = inUserInput.substr(0, inCommandEndPos);
  std::vector<std::string> parsed = ParseCommandParameters(raw_command);
  if (!parsed.empty())
    return parsed[0];
  return raw_command;
}

std::vector<std::string> GetCommandParameters(const std::string& inUserInput, const size_t& inCommandEndPos)
{
  size_t parameter_start_pos = inCommandEndPos + 1;
  if (parameter_start_pos >= inUserInput.length())
    return {};

  return ParseCommandParameters(inUserInput.substr(parameter_start_pos));
}

void GetSystemPaths(std::vector<std::filesystem::path>& outSystemPaths)
{
  std::string env_paths = GetEnvironmentVariable("PATH");

  auto split_paths = env_paths | std::views::split(PATH_DELIMITER);
  for (const auto& path : split_paths)
  {
    std::filesystem::path parsed_path(path.begin(), path.end());

    if (PathExists(parsed_path))
    {
      outSystemPaths.push_back(parsed_path);
    }
  }
}

void GetExecutableDirectory(const std::string& inCommandName, const std::vector<std::filesystem::path>& inPathsToLook, std::filesystem::path& outExecutableDirectory)
{
  for (const std::filesystem::path& path : inPathsToLook)
  {
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(path, std::filesystem::directory_options::skip_permission_denied))
    {
      if (inCommandName == entry.path().stem().string() || inCommandName == entry.path().filename().string())
      {
        if (PathExists(entry.path()) && PathIsExecutable(entry.path()))
        {
          outExecutableDirectory = entry.path();
          return;
        }
      }
    }
  }
}

void FindExecutablePath(const std::string& inName, std::filesystem::path& outPath)
{
  std::filesystem::path direct_path(inName);
  if ((inName.find('/') != std::string::npos || inName.find('\\') != std::string::npos || PathExists(direct_path)) && PathIsExecutable(direct_path))
  {
    outPath = direct_path;
    return;
  }

  std::vector<std::filesystem::path> system_paths;
  GetSystemPaths(system_paths);

  if (!system_paths.empty())
  {
    GetExecutableDirectory(inName, system_paths, outPath);
  }
}

bool IsShellBuiltin(const std::string& inName, const std::map<const std::string_view, const CommandId>& inCommands)
{
  for (const auto& [command_name, command_id] : inCommands)
  {
    if (inName == command_name)
    {
      return true;
    }
  }
  return false;
}

void ExecuteType(std::vector<std::string>& inParameters, const std::map<const std::string_view, const CommandId>& inCommands)
{
  std::string parameter;
  for (size_t i = 0; i < inParameters.size(); ++i)
  {
    parameter.append(inParameters[i]);
    if (i + 1 < inParameters.size())
      parameter.append(" ");
  }

  if (IsShellBuiltin(parameter, inCommands))
  {
    std::cout << parameter << " is a shell builtin" << std::endl;
    return;
  }

  std::filesystem::path executable_path;
  FindExecutablePath(parameter, executable_path);

  if (!executable_path.empty())
  {
    std::cout << parameter << " is " << executable_path.string() << std::endl;
    return;
  }

  std::cout << parameter << ": not found" << std::endl;
}

void RunExecutable(const std::filesystem::path& inPath, std::vector<std::string>& inParameters)
{
  pid_t pid = fork();

  if (pid == 0)
  {
    std::vector<char*> exec_args;

    std::string temp_name = inPath.filename().string();
    exec_args.push_back(temp_name.data());

    for (auto& parameter : inParameters)
    {
      exec_args.push_back(parameter.data());
    }

    exec_args.push_back(nullptr);

    execvp(inPath.c_str(), exec_args.data());

    std::_Exit(EXIT_FAILURE);
  }
  else if (pid > 0)
  {
    int status;
    waitpid(pid, &status, 0);
  }
  else
  {
    std::cout << "Failed to fork process" << std::endl;
  }
}

bool ExecuteExternal(const std::string& inName, std::vector<std::string>& inParameters)
{
  std::filesystem::path executable_path;
  FindExecutablePath(inName, executable_path);

  if (!executable_path.empty())
  {
    RunExecutable(executable_path, inParameters);
    return true;
  }
  return false;
}

void ExecuteEcho(const std::vector<std::string>& inParameters)
{
  for (size_t i = 0; i < inParameters.size(); ++i)
  {
    std::cout << inParameters[i];
    if (i + 1 < inParameters.size())
    {
      std::cout << " ";
    }
  }
  std::cout << std::endl;
}

void ExecutePwd()
{
  std::cout << std::filesystem::current_path().string() << std::endl;
}

void ExecuteCd(std::vector<std::string>& inParameters)
{
  std::string parameter;
  for (size_t i = 0; i < inParameters.size(); ++i)
  {
    parameter.append(inParameters[i]);
    if (i + 1 < inParameters.size())
      parameter.append(" ");
  }

  std::filesystem::path directory;
  if (parameter == "~")
  {
    directory = GetHomeDirectory();
  }
  else if (parameter.rfind("~/", 0) == 0)
  {
    directory = GetHomeDirectory() + parameter.substr(1);
  }
  else
  {
    directory = parameter;
  }

  if (PathExists(directory))
  {
    std::filesystem::current_path(directory);
    return;
  }

  std::cout << "cd: " << directory.string() << ": No such file or directory" << std::endl;
}

int main()
{
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  const bool is_running = true;

  std::map<const std::string_view, const CommandId> commands = {
    {"exit", CommandId::Exit},
    {"echo", CommandId::Echo},
    {"type", CommandId::Type},
    {"pwd", CommandId::Pwd},
    {"cd", CommandId::Cd}
  };

  while (is_running)
  {
    std::cout << "$ ";

    std::string input;
    if (!std::getline(std::cin, input))
    {
      break;
    }

    if (!input.empty() && input.back() == '\r')
    {
      input.pop_back();
    }

    if (input.empty())
    {
      continue;
    }

    const std::size_t command_end_pos = GetCommandEndPos(input);
    const std::string command_section = GetCommandValue(input, command_end_pos);
    std::vector<std::string> parameters = GetCommandParameters(input, command_end_pos);

    if (std::map<const std::string_view, const CommandId>::iterator iterator = commands.find(command_section); iterator != commands.end())
    {
      switch (iterator->second)
      {
        case CommandId::Exit:
          exit(0);
        case CommandId::Echo:
          ExecuteEcho(parameters);
          continue;
        case CommandId::Type:
          ExecuteType(parameters, commands);
          continue;
        case CommandId::Pwd:
          ExecutePwd();
          continue;
        case CommandId::Cd:
          ExecuteCd(parameters);
          continue;
      }
    }
    else
    {
      if (ExecuteExternal(command_section, parameters))
      {
        continue;
      }

      std::cout << input << ": command not found" << std::endl;
    }
  }
}