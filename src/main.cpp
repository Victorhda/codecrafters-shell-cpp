#include <iostream>
#include <string>
#include <array>
#include <string_view>


size_t GetCommandEndPos(const std::string& inUserInput)
{
  std::string_view command_delimiter = " ";
  std::size_t command_end_pos = inUserInput.find(command_delimiter);

  if (command_end_pos != std::string::npos)
  {
    return command_end_pos;
  }

  return inUserInput.length();
}


std::string GetCommandValue(const std::string& inUserInput, const size_t& inCommandEndPos)
{
  return inUserInput.substr(0, inCommandEndPos);
}


std::string GetCommandParameters(const std::string& inUserInput, const size_t& inCommandEndPos)
{
  return inUserInput.substr(inCommandEndPos + 1);
}


int main() 
{
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  bool is_running = true;

  std::string_view exit_command = "exit";
  std::string_view echo_command = "echo";
  std::string_view type_command = "type";

  std::array<std::string_view, 3> commands = {exit_command, echo_command, type_command};
  
  while (is_running)
  {
    std::cout << "$ ";

    // Get user input as string
    std::string input;
    std::getline(std::cin, input);

    size_t command_end_pos = GetCommandEndPos(input);
    std::string command = GetCommandValue(input, command_end_pos);
    std::string parameter = GetCommandParameters(input, command_end_pos);

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
          
        }
      }

      if (is_valid)
        std::cout << parameter << " is a shell builtin" << "\n";
      else
        std::cout << parameter << ": not found" << "\n";
    }
    else
    {
      std::cout << input << ": command not found";
      std::cout << "\n";
      continue;
    }
  }
}
