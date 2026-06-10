#include "DataGrouper.h"
#include "structs.h"
DataGrouper::DataGrouper(std::string& path) {
	this->path = path;
}

void DataGrouper::saveInCSV(std::vector<std::vector<std::vector<PlyUtilities::coloredPoint>>> grooves, int i) {
    std::ofstream f(path + "output" + std::to_string(i) +".csv");
    f << "groove,line,x,y,z,r,g,b\n";

    for (int g = 0; g < (int)grooves.size(); g++)
        for (int l = 0; l < (int)grooves[g].size(); l++)
            for (auto& p : grooves[g][l])
                f << g << "," << l << ","
                << p.x << "," << p.y << "," << p.z << ","
                << (int)p.r << "," << (int)p.g << "," << (int)p.b << "\n";

    f.close();
    std::cout << "Saved to " << path << "output.csv\n";
}

std::vector<std::vector<std::vector<PlyUtilities::coloredPoint>>> DataGrouper::loadFromCSV() {
    std::vector<std::vector<std::vector<PlyUtilities::coloredPoint>>> out;

    std::ifstream f(path + "outputC.csv");
    if (!f.is_open()) {
        std::cout << "Failed to open " << path << "output.csv\n";
        return out;
    }

    std::string line;
    std::getline(f, line);  // skip header

    int prevG = -1, prevL = -1;

    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string token;

        int g, l, r, gb, b;
        float x, y, z;

        std::getline(ss, token, ','); g = std::stoi(token);
        std::getline(ss, token, ','); l = std::stoi(token);
        std::getline(ss, token, ','); x = std::stof(token);
        std::getline(ss, token, ','); y = std::stof(token);
        std::getline(ss, token, ','); z = std::stof(token);
        std::getline(ss, token, ','); r = std::stoi(token);
        std::getline(ss, token, ','); gb = std::stoi(token);
        std::getline(ss, token, ','); b = std::stoi(token);

        PlyUtilities::coloredPoint p;
        p.x = x; p.y = y; p.z = z;
        p.r = (unsigned char)r;
        p.g = (unsigned char)gb;
        p.b = (unsigned char)b;

        // Expand outer vector if needed
        while ((int)out.size() <= g)
            out.push_back({});

        // Expand groove vector if needed
        while ((int)out[g].size() <= l)
            out[g].push_back({});

        out[g][l].push_back(p);
    }

    f.close();
    std::cout << "Loaded from " << path << "output.csv\n";
    return out;
}

void DataGrouper::saveFTDToCSV(std::vector<std::vector<FTD>> ftData, int i) {
    std::ofstream f(path + "ftd_output" + std::to_string(i) + ".csv");
    f << "groove,line,frequency,amplitude,phase\n";

    for (int g = 0; g < (int)ftData.size(); g++)
        for (int l = 0; l < (int)ftData[g].size(); l++)
            for (int i = 0; i < (int)ftData[g][l].f.size(); i++)
                f << g << "," << l << ","
                << ftData[g][l].f[i] << ","
                << ftData[g][l].amplitude[i] << ","
                << ftData[g][l].phase[i] << "\n";

    f.close();
    std::cout << "Saved FTD to " << path << "ftd_output.csv\n";
}