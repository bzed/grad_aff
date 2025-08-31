#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include "grad_aff/pbo/Pbo.h"
#include "grad_aff/paa/paa.h"
#include "grad_aff/wrp/wrp.h"
#include "grad_aff/p3d/odol.h"

namespace fs = std::filesystem;

// Simple print help function
void printHelp() {
    std::cout << "grad_aff CLI Tool" << std::endl;
    std::cout << "Usage: grad_aff_cli <command> [options]" << std::endl << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  pbo info <pbo_file>                 Show information about a PBO file." << std::endl;
    std::cout << "  pbo extract <pbo_file> <out_dir>    Extract a PBO file to the target directory." << std::endl;
    std::cout << "  paa info <paa_file>                 Show information about a PAA file." << std::endl;
#ifdef GRAD_AFF_USE_OIIO
    std::cout << "  paa to-png <paa_file> <out_png>     Convert a PAA file to a PNG image." << std::endl;
    std::cout << "  paa from-png <in_png> <out_paa>     Convert a PNG image to a PAA file." << std::endl;
#endif
    std::cout << "  p3d info <p3d_file>                 Show information about a P3D model file." << std::endl;
    std::cout << "  wrp info <wrp_file>                 Show information about a WRP file." << std::endl;
    std::cout << "  help                                Show this help message." << std::endl;
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

            std::cout << "Extracting " << pbo.entries.size() << " files to " << fs::absolute(outDir) << "..." << std::endl;

            for (const auto& entryPair : pbo.entries) {
                const auto& entry = entryPair.second;
                std::string entryPathStr = entry->filename.string();
                std::replace(entryPathStr.begin(), entryPathStr.end(), '\\', fs::path::preferred_separator);

                fs::path finalOutPath = outDir / entryPathStr;
                fs::path finalOutDir = finalOutPath.parent_path();

                if (!finalOutDir.empty() && !fs::exists(finalOutDir)) {
                    fs::create_directories(finalOutDir);
                }

                std::ofstream ofs(finalOutPath, std::ios::binary);
                if (ofs) {
                    ofs.write(reinterpret_cast<const char*>(entry->data.data()), entry->data.size());
                    ofs.close();
                } else {
                    std::cerr << "Error: Could not open file for writing: " << finalOutPath << std::endl;
                }
            }

            std::cout << "Successfully extracted " << pbo.entries.size() << " files." << std::endl;

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
    fs::path inputFile = args[2];

    if (!fs::exists(inputFile)) {
        std::cerr << "Error: Input file does not exist: " << inputFile << std::endl;
        return;
    }

    grad_aff::Paa paa;

    try {
        if (action == "info") {
            paa.readPaa(inputFile.string(), true); // Peek to read headers
            std::cout << "PAA Info: " << inputFile.filename() << std::endl;
            if (!paa.mipMaps.empty()) {
                 std::cout << "  Dimensions: " << paa.mipMaps[0].width << "x" << paa.mipMaps[0].height << std::endl;
            }
            std::cout << "  Mipmap levels: " << paa.mipMaps.size() << std::endl;
            std::cout << "  Has transparency: " << (paa.hasTransparency ? "Yes" : "No") << std::endl;
        }
#ifdef GRAD_AFF_USE_OIIO
        else if (action == "to-png") {
            if (args.size() < 4) {
                std::cerr << "Error: Output PNG file not specified." << std::endl;
                return;
            }
            fs::path outImage = args[3];
            paa.readPaa(inputFile.string());
            paa.writeImage(outImage.string());
            std::cout << "Successfully converted " << inputFile.filename() << " to " << outImage.filename() << std::endl;
        } else if (action == "from-png") {
            if (args.size() < 4) {
                std::cerr << "Error: Output PAA file not specified." << std::endl;
                return;
            }
            fs::path outPaa = args[3];
            paa.readImage(inputFile.string());
            paa.writePaa(outPaa.string());
            std::cout << "Successfully converted " << inputFile.filename() << " to " << outPaa.filename() << std::endl;
        }
#endif
        else {
             std::cerr << "Error: Unknown action '" << action << "' for paa command." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
    }
}

// Handler for P3D commands
void handleP3d(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "Error: Not enough arguments for 'p3d' command." << std::endl;
        printHelp();
        return;
    }
    std::string action = args[1];
    fs::path p3dFile = args[2];

    if (!fs::exists(p3dFile)) {
        std::cerr << "Error: Input file does not exist: " << p3dFile << std::endl;
        return;
    }

    try {
        if (action == "info") {
            grad_aff::Odol odol(p3dFile.string());
            odol.readOdol(false); // Read metadata without full LOD parsing
            std::cout << "P3D Info: " << p3dFile.filename() << std::endl;
            std::cout << "  ODOL Version: " << odol.version << std::endl;
            std::cout << "  LODs: " << odol.modelInfo.nLods << std::endl;
            if (odol.modelInfo.animated) {
                std::cout << "  Skeleton: " << odol.modelInfo.skeleton.name << " (" << odol.modelInfo.skeleton.nBones << " bones)" << std::endl;
            }
            // To show textures, you would need to read at least one LOD
            // For example, reading the first visual LOD to list its textures
            odol.readOdol(true);
            if(!odol.lods.empty() && !odol.lods[0].textures.empty()) {
                std::cout << "  Textures in first LOD:" << std::endl;
                for(const auto& texture : odol.lods[0].textures) {
                    std::cout << "    - " << texture << std::endl;
                }
            }

        } else {
            std::cerr << "Error: Unknown action '" << action << "' for p3d command." << std::endl;
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
    } else if (command == "p3d") {
        handleP3d(args);
    }
    else if (command == "wrp") {
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
