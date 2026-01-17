#include <iostream>
#include <fstream>
#include <filesystem>

int main() {
    std::string path = "assets/accounts.txt";

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "❌ Could not open file.\n";
    }
    else {
        std::cout << "✅ File opened successfully.\n";

        std::string line;

        return 0;
    }
}
