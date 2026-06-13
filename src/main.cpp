#include <iostream>
#include <string>


int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::cout << "$ ";
  
  // Get user input as string
  std::string input;
  std::cin >> input;

  // Output command not found
  std::cout << input << ": command not found";
}
