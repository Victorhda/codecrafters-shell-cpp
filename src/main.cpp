#include <iostream>
#include <string>
#include <array>
#include <string_view>
#include <filesystem>
#include <ranges>
#include <system_error>


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
  
  char delimiter = ';';

  auto split_paths = env_paths | std::views::split(delimiter);
  for (const auto& path : split_paths)
  {
    std::string_view system_path(path.data(), path.size());
    std::filesystem::path parsed_path(system_path);
    
    std::error_code error_code;
    std::filesystem::file_status path_status = std::filesystem::status(parsed_path, error_code);

    if (error_code.value() == 0 && std::filesystem::exists(path_status))
    {
      outSystemPaths.push_back(parsed_path);
    }
  }
}


void GetExecutableDirectory(const std::string& inCommandName, const std::vector<std::filesystem::path>& inPathsToLook, std::filesystem::path& outExecutableDirectory)
{
  std::string command_with_exe = inCommandName;
  if (command_with_exe.rfind(".exe") == std::string::npos) 
  {
      command_with_exe += ".exe";
  }

  for (const std::filesystem::path& path : inPathsToLook)
  {
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(path, std::filesystem::directory_options::skip_permission_denied))
    {
      if(inCommandName == entry.path().filename().string() || command_with_exe == entry.path().filename().string())
      {
        std::error_code error_code;
        std::filesystem::file_status file_status = std::filesystem::status(entry.path(), error_code);

        if (error_code.value() == 0 && std::filesystem::is_regular_file(file_status))
        {
            outExecutableDirectory = entry.path();
            return; 
        }
      }
    }
  }
}


int main() 
{
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  const bool is_running = true;

  const std::string_view exit_command = "exit";
  const std::string_view echo_command = "echo";
  const std::string_view type_command = "type";

  std::array<const std::string_view, 3> commands = {exit_command, echo_command, type_command};
  
  while (is_running)
  {
    std::cout << "$ ";

    // Get user input as string
    std::string input;
    std::getline(std::cin, input);

    const std::size_t command_end_pos = GetCommandEndPos(input);
    const std::string command = GetCommandValue(input, command_end_pos);
    const std::string parameter = GetCommandParameters(input, command_end_pos);

    if (command == exit_command)
    {
      exit(0);
    }
    else if (command == echo_command)
    {
      std::cout << parameter << "\n";
    }
    else if (command == type_command)
    {
      bool is_valid = false;

      for (std::string_view available_command: commands)
      {
        if (parameter == available_command)
        {
          is_valid = true;   
          break;
        }
      }

      if (is_valid)
      {
        std::cout << parameter << " is a shell builtin" << "\n";
        continue;
      }

      std::vector<std::filesystem::path> system_paths; 
      GetSystemPaths(system_paths);

      std::filesystem::path executable_directory;
      GetExecutableDirectory(parameter, system_paths, executable_directory);
      if (!executable_directory.empty())
      {
        std::cout << parameter << " is " << executable_directory << "\n";
        continue;
      }
      
      std::cout << parameter << ": not found" << "\n";
      continue;
    }
    else
    {
      std::cout << input << ": command not found";
      std::cout << "\n";
      continue;
    }
  }
}
