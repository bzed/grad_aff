#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include "grad_aff/pbo/Pbo.h"
#include "grad_aff/paa/paa.h"
#include "grad_aff/wrp/wrp.h"

namespace fs = std::filesystem;

// Simple print help function
void printHelp() {
    std::cout << "grad_aff CLI Tool" << std::endl;
    std::cout << "Usage: grad_aff_cli <command> [options]" << std::endl << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  pbo info <pbo_file>          Show information about a PBO file." << std::endl;
    std::cout << "  pbo extract <pbo_file> <out_dir>  Extract a PBO file." << std::endl;
    std::cout << "  paa info <paa_file>          Show information about a PAA file." << std::endl;
    std::cout << "  wrp info <wrp_file>          Show information about a WRP file." << std::endl;
    std::cout << "  help                         Show this help message." << std::endl;
}

// Handler for PBO commands
void handlePbo(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "Error: Not enough arguments for 'pbo' command." << std::endl;
        printHelp();
        return;
    }

    std::string action = args[1];
    fs::path pboFile = args[2];

    if (!fs::exists(pboFile)) {
        std::cerr << "Error: Input file does not exist: " << pboFile << std::endl;
        return;
    }

    grad_aff::Pbo pbo(pboFile.string());

    try {
        if (action == "info") {
            pbo.readPbo(false); // Read headers only
            std::cout << "PBO Info: " << pbo.pboName << std::endl;
            std::cout << "  " << pbo.entries.size() << " file entries." << std::endl;
            std::cout << "  Product Entries:" << std::endl;
            for (const auto& entry : pbo.productEntries) {
                std::cout << "    " << entry.first << ": " << entry.second << std::endl;
            }
        } else if (action == "extract") {
            if (args.size() < 4) {
                std::cerr << "Error: Output directory not specified for extraction." << std::endl;
                return;
            }
            fs::path outDir = args[3];
            pbo.readPbo(true); // Read with data
            pbo.extractPbo(outDir);
            std::cout << "Successfully extracted " << pboFile.filename() << " to " << outDir << std::endl;
        } else {
            std::cerr << "Error: Unknown action '" << action << "' for pbo command." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
    }
}

// Handler for PAA commands
void handlePaa(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "Error: Not enough arguments for 'paa' command." << std::endl;
        printHelp();
        return;
    }
    std::string action = args[1];
    fs::path paaFile = args[2];

    if (!fs::exists(paaFile)) {
        std::cerr << "Error: Input file does not exist: " << paaFile << std::endl;
        return;
    }

    grad_aff::Paa paa;

    try {
        if (action == "info") {
            paa.readPaa(paaFile.string(), true); // Peek to read headers
            std::cout << "PAA Info: " << paaFile.filename() << std::endl;
            if (!paa.mipMaps.empty()) {
                 std::cout << "  Dimensions: " << paa.mipMaps[0].width << "x" << paa.mipMaps[0].height << std::endl;
            }
            std::cout << "  Mipmap levels: " << paa.mipMaps.size() << std::endl;
            std::cout << "  Has transparency: " << (paa.hasTransparency ? "Yes" : "No") << std::endl;
        } else {
             std::cerr << "Error: Unknown action '" << action << "' for paa command." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
    }
}

// Handler for WRP commands
void handleWrp(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "Error: Not enough arguments for 'wrp' command." << std::endl;
        printHelp();
        return;
    }
    std::string action = args[1];
    fs::path wrpFile = args[2];

    if (!fs::exists(wrpFile)) {
        std::cerr << "Error: Input file does not exist: " << wrpFile << std::endl;
        return;
    }
    
    try {
        if (action == "info") {
            grad_aff::Wrp wrp(wrpFile.string());
            wrp.readWrp();
            std::cout << "WRP Info: " << wrp.wrpName << std::endl;
            std::cout << "  Version: " << wrp.wrpVersion << std::endl;
            std::cout << "  Map Size: " << wrp.mapSizeX << "x" << wrp.mapSizeY << std::endl;
            std::cout << "  Object count: " << wrp.objects.size() << std::endl;
            std::cout << "  Model count: " << wrp.models.size() << std::endl;
        } else {
            std::cerr << "Error: Unknown action '" << action << "' for wrp command." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
    }
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 1;
    }

    std::vector<std::string> args(argv + 1, argv + argc);
    std::string command = args[0];

    if (command == "pbo") {
        handlePbo(args);
    } else if (command == "paa") {
        handlePaa(args);
    } else if (command == "wrp") {
        handleWrp(args);
    } else if (command == "help" || command == "--help" || command == "-h") {
        printHelp();
    } else {
        std::cerr << "Error: Unknown command '" << command << "'" << std::endl;
        printHelp();
        return 1;
    }

    return 0;
}
