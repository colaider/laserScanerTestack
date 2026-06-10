#pragma once
#include <string>
#include <iostream>
#include <vector>
#include "PlyUtilities.h"



class CSVUtils {

private:
	struct batch {
		std::vector<float> areas;
		std::vector<float> widths;
	};
	struct groveData {
		std::vector<batch> batches;
		int batchLength;
	};

	std::string filePath;
	groveData gData;

public:
	CSVUtils(const std::string& filePath);
	void writeToCSV(std::vector<float> area, std::vector<float> width, int batchLength, bool hole);
	void createAVersionFolder(PlyUtilities *plyUtils, const std::string& versionName);
	void arrangeBatches(std::vector<float> area, std::vector<float> width, int batchLength);
};