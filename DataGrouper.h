#pragma once
#include <string>
#include "PlyUtilities.h"
#include "structs.h"

class DataGrouper {
	std::string path;


public:
	DataGrouper(std::string & path);
	void saveInCSV(std::vector<std::vector<std::vector<PlyUtilities::coloredPoint>>> grooves, int i);
	std::vector<std::vector<std::vector<PlyUtilities::coloredPoint>>> loadFromCSV();
	void DataGrouper::saveFTDToCSV(std::vector<std::vector<FTD>> ftData, int i);
};