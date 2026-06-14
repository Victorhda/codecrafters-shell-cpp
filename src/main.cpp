#include <iostream>
#include <string>
#include <array>
#include <string_view>
#include <filesystem>
#include <ranges>
#include <system_error>
#include <vector>
#include <map>


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
  size_t parameter_start_pos = inCommandEndPos + 1;
  if (inUserInput.length() >= parameter_start_pos)
  {
    return inUserInput.substr(parameter_start_pos);
  }
  else
    return "";
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
    std::cout << inCommandName << " is a shell builtin" << std::endl; 
    return;
  }

  std::filesystem::path executable_path;
  FindExecutablePath(inCommandName, executable_path);

  if (!executable_path.empty())
  {
    std::cout << inCommandName << " is " << executable_path.replace_extension("").string() << std::endl;
    return;
  }

  std::cout << inCommandName << ": not found" << std::endl;
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


bool CharIsQuote(char& inChar)
{
  if (inChar == '\'' || inChar == '\"')
  {
    return true;
  }
  return false;
}


bool CharIsEmpty(char& inChar)
{
  if (inChar == ' ')
  {
    return true;
  }
  return false;
}


void ExecuteEcho(std::string& inMessage)
{
  bool inside_quotation = false;

  for (int index = 0; index < inMessage.length();)
  {
    if (CharIsQuote(inMessage[index]))
    {
      if (CharIsQuote(inMessage[index+1]))
      {
        inMessage.erase(index, 2);
        continue;
      }

      if (!inside_quotation)
      {
        inside_quotation = true;
        inMessage.erase(index, 1);
        continue;
      }
      else
      {
        inside_quotation = false;
        inMessage.erase(index, 1);
        continue;
      }
    }

    if (!inside_quotation)
    {
      if (CharIsEmpty(inMessage[index]) && CharIsEmpty(inMessage[index+1]))
      {
        inMessage.erase(index, 1);
        continue;
      }
    }
    index++;
  }
  std::cout << inMessage << std::endl;
}


void ExecutePwd()
{
  std::cout << std::filesystem::current_path().string() << std::endl;
}


void ExecuteCd(const std::string& inArgument)
{
  std::filesystem::path directory;
  if (inArgument == "~")
  {
    directory = GetHomeDirectory();
  }
  else
  {
    directory = inArgument;
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
    std::string parameter_section = GetCommandParameters(input, command_end_pos);

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
      
      std::cout << input << ": command not found" << std::endl;
    }
  }
}
