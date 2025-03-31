#include <iostream>
#include <nlohmann/json.hpp>

void test() {
    nlohmann::json j;
    j["name"] = "John Doe";
    j["age"] = 30;
    j["city"] = "New York";

    std::cout << j.dump(4) << std::endl; // Pretty print with 4 spaces
}

int main() {
    std::cout << "Hello, World!" << std::endl;
    test();

    return 0;
}
