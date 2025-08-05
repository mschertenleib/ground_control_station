#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main()
{
    try
    {
        std::cout << "Hello, wanderer!\n";

        return EXIT_SUCCESS;
    }
    catch (const std::exception &e)
    {
        std::cout.flush();
        std::cerr << "Exception thrown: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cout.flush();
        std::cerr << "Unknown exception thrown\n";
        return EXIT_FAILURE;
    }
}
