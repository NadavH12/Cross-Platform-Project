#ifndef _DETERMINE_OS
#define _DETERMINE_OS
#include <iostream>
#include "windowsProject.cpp"
//#include "macProject.cpp"

int main(){

    #if defined(__linux__) // Or #if __linux__
        std::cout << "Hello, GNU/Linux!" << '\n';
    #elif __APPLE__
        std::cout << "Hello, macOS!" << '\n';
    #elif _WIN32 || _WIN64
        std::cout << "Hello, Windows 32 or Windows 64!" << '\n';
        windows();
    #endif

}
#endif