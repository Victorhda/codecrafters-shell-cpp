#include <iostream>
#include <string>


int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  

  bool is_running = true;
  
  while (is_running)
  {
    std::cout << "$ ";

    // Get user input as string
    std::string input;
    std::cin >> input;

    std::string exit_command = "exit";

    // Output command not found
    if (input == exit_command)
      exit(0);

    std::cout << input << ": command not found";
    
    std::cout << "\n";
  }
  
}
