#include <iostream>
#include <string>
#include <array>
#include <string_view>


size_t GetCommandPos(const std::string& inUserInput, const std::array<std::string_view, 2> inCommands)
{
  std::size_t command_pos = std::string::npos;
  std::string_view delimiter = " ";

  for (std::string_view command: inCommands)
  {
    command_pos = inUserInput.find(command);
   
    if (command_pos != std::string::npos)
      return command_pos + command.length();
  }
  return command_pos;
}


std::string GetCommandValue(const std::string& inUserInput, const size_t& inCommandPos)
{
  return inUserInput.substr(0, inCommandPos);
}


std::string GetCommandParameters(const std::string& inUserInput, const size_t& inCommandPos)
{
  return inUserInput.substr(inCommandPos);
}


int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  bool is_running = true;

  std::string_view exit_command = "exit";
  std::string_view echo_command = "echo ";

  std::array<std::string_view, 2> commands = {exit_command, echo_command};
  
  while (is_running)
  {
    std::cout << "$ ";

    // Get user input as string
    std::string input;
    std::getline(std::cin, input);

    size_t command_pos = GetCommandPos(input, commands);
    if (command_pos == std::string::npos)
    {
      std::cout << input << ": command not found";
      std::cout << "\n";
      continue;
    }
      
    std::string command = GetCommandValue(input, command_pos);
    std::string parameter = GetCommandParameters(input, command_pos);

    if (command == exit_command)
    {
      exit(0);
    }
    else if (command == echo_command)
    {
      std::cout << parameter << "\n";
    }
  }
}
