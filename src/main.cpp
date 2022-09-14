#include <iostream>
#include <string>
#include <vector>
#include <fstream>

#include <Eigen/Dense>


class district {
    public:
    std::string name_;
    // int id_;
    // int population_;
    int seats_;
    district(const std::string name, const int seats) : name_(name), seats_(seats) {}
};

class party {
    public:
    std::string name;
    int seats;
    int share;
};

class election {
    public:
    std::string name;
    std::vector<district> districts;
};


// reads pivot table from csv file and stores data 
void loadCSV(Eigen::MatrixXi &data, std::vector<std::string> &parties, std::vector<district> &districts, const std::string path, const char delim = ';') {
    using namespace Eigen;

    std::ifstream in(path);
    std::vector<int> values; // will be converted to data matrix
    std::string line, cell;

    // read in parties
    std::getline(in, line);
    std::stringstream lineStream(line);
    // throw away first two cells
    std::getline(lineStream, cell, delim);
    std::getline(lineStream, cell, delim);
    while(std::getline(lineStream, cell, delim)) {
        parties.push_back(cell);
    }

    // read in next lines (votes)
    unsigned rows = 0;
    while (std::getline(in, line)) {
        std::stringstream lineStream(line);
        std::string dist, seats;
        std::getline(lineStream, dist, delim);
        std::getline(lineStream, seats, delim);
        districts.emplace_back(district(dist, std::stoi(seats)));    // add first two entries two votes vector
        
        while (std::getline(lineStream, cell, delim)) {
            values.push_back(std::stoi(cell));
        }
        ++rows;

    }
    data = Map<Matrix<int, Dynamic, Dynamic, RowMajor>>(values.data(), rows, values.size()/rows);

}


int main(int argc, char** argv) {
    // import 
    Eigen::MatrixXi data;
    std::vector<std::string> parties;
    std::vector<district> districts;
    loadCSV(data, parties, districts, "../data/2019_KRZH.csv", ';');

    // min quorum


    // Oberzuteilung


    // Unterzuteilung

}
