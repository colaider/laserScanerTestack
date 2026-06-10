#include "CSVUtils.h"
#include <fstream>

CSVUtils::CSVUtils(const std::string& filePath){
	this->filePath = filePath;
}

void CSVUtils::arrangeBatches(std::vector<float> area, std::vector<float> width, int batchLength) {
	int totalBatches = (int)ceil((float)area.size() / batchLength);
	batch b;

	gData.batchLength = batchLength;
	for (int i = 0; i < totalBatches; i++) {
		b.areas.clear(); b.widths.clear();	
		for(int j = i*batchLength; j < ((i+1)*batchLength <= area.size() ? (i+1)*batchLength : area.size()); j++){
			b.areas.push_back(area[j]);
			b.widths.push_back(width[j]);
		}
		gData.batches.push_back(b);
	}
}
//sadasdsaddasd
void CSVUtils::writeToCSV(std::vector<float> area, std::vector<float> width, int batchLength, bool hole) {
    this->arrangeBatches(area, width, batchLength);

    std::ofstream file(this->filePath);
    if (!file.is_open()) { std::cerr << "Failed to open file: " << this->filePath << std::endl; return; }

    file << "Batch";
    for (int j = 0; j < batchLength; j++)
        file << ",Area_" << (j + 1) << (!hole ? ",Width_" : ",Diameter_") << (j + 1);
    file << "\n";

    for (size_t i = 0; i < gData.batches.size(); i++) {
        file << (i + 1);
        for (size_t j = 0; j < gData.batches[i].areas.size(); j++)
            file << "," << gData.batches[i].areas[j]
            << "," << gData.batches[i].widths[j];
        for (size_t j = gData.batches[i].areas.size(); j < (size_t)batchLength; j++)
            file << ",,";
        file << "\n";
    }

    file.close();
    std::cout << "Saved to " << this->filePath << "\n";
}

void CSVUtils::createAVersionFolder(PlyUtilities *plyUtils, const std::string& versionName) {
    
}
