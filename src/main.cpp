#include <iostream>
#include <string>
#include <array>
#include <string_view>
#include <filesystem>
#include <ranges>
#include <system_error>
#include <vector>
#include <map>


enum CommandId
{
  Exit,
  Echo,
  Type,
  Pwd,
  Cd
};


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


const std::size_t GetCommandEndPos(const std::string& inUserInput)
{
  char command_delimiter = ' ';
  std::size_t command_end_pos = inUserInput.find(command_delimiter);

  if (command_end_pos != std::string::npos)
  {
    return command_end_pos;
  }

  return inUserInput.length();
}


const std::string GetCommandValue(const std::string& inUserInput, const size_t& inCommandEndPos)
{
  return inUserInput.substr(0, inCommandEndPos);
}


const std::string GetCommandParameters(const std::string& inUserInput, const size_t& inCommandEndPos)
{
  if (inUserInput.length() >= inCommandEndPos + 1)
  {
    return inUserInput.substr(inCommandEndPos + 1);
  }
  else
    return "";
}


void GetSystemPaths(std::vector<std::filesystem::path>& outSystemPaths)
{
  std::string env_paths = std::getenv("PATH");
  
  char delimiter;
  if (env_paths.find(';') != std::string::npos)
    delimiter = ';';
  else if (env_paths.find(':') != std::string::npos)
    delimiter = ':';

  auto split_paths = env_paths | std::views::split(delimiter);
  for (const auto& path : split_paths)
  {
    std::string_view system_path(path.data(), path.size());
    std::filesystem::path parsed_path(system_path);
    
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
      if(inCommandName == entry.path().stem().string() || inCommandName == entry.path().filename().string())
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
  std::vector<std::filesystem::path> system_paths; 
  GetSystemPaths(system_paths);

  if (system_paths.size() > 0)
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


void ExecuteType(const std::string& inCommandName, const std::map<const std::string_view, const CommandId>& inCommands)
{
  if (IsShellBuiltin(inCommandName, inCommands))
  {
    std::cout << inCommandName << " is a shell builtin" << "\n"; 
    return;
  }

  std::filesystem::path executable_path;
  FindExecutablePath(inCommandName, executable_path);

  if (!executable_path.empty())
  {
    std::cout << inCommandName << " is " << executable_path.replace_extension("").string() << "\n";
    return;
  }

  std::cout << inCommandName << ": not found" << "\n";
}


void RunExecutable(const std::filesystem::path& inPath, const std::string& inArguments)
{
  std::string command = "\"" + inPath.stem().string() + "\" " + inArguments;
  system(command.c_str());
}


bool ExecuteExternal(const std::string& inName, const std::string& inArguments)
{
  std::filesystem::path executable_path;
  FindExecutablePath(inName, executable_path);

  if (!executable_path.empty())
  {
    RunExecutable(executable_path, inArguments);
    return true;
  }
  return false;
}


void ExecuteEcho(const std::string& inMessage)
{
  std::cout << inMessage << "\n";
}


void ExecutePwd()
{
  std::cout << std::filesystem::current_path().string() << "\n";
}

void ExecuteCd(const std::string& inDirectoryName)
{
  std::filesystem::path directory(inDirectoryName);
  if (PathExists(directory))
  {
    std::filesystem::current_path(directory);
    return;
  }

  std::cout << "cd: " << directory.string() << ": No such file or directory" << "\n";
}


int main() 
{
  // Flush after every std::cout / std:cerr
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

    // Get user input as string
    std::string input;
    std::getline(std::cin, input);

    const std::size_t command_end_pos = GetCommandEndPos(input);
    const std::string command_section = GetCommandValue(input, command_end_pos);
    const std::string parameter_section = GetCommandParameters(input, command_end_pos);

    if (std::map<const std::string_view, const CommandId>::iterator iterator = commands.find(command_section); iterator != commands.end())
    {
        switch (iterator->second)
        {
          case CommandId::Exit:
            exit(0);
          case CommandId::Echo:
            ExecuteEcho(parameter_section);
            continue;
          case CommandId::Type:
            ExecuteType(parameter_section, commands);
            continue;
          case CommandId::Pwd:
            ExecutePwd();
            continue;
          case CommandId::Cd:
            ExecuteCd(parameter_section);
            continue;
        }
    }
    else
    {
      if (ExecuteExternal(command_section, parameter_section))
      {
          continue;
      }
      
      std::cout << input << ": command not found" << "\n";
    }
  }
}
