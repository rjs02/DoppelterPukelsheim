#include <iostream>
#include <string>
#include <vector>
#include <fstream>

#include <Eigen/Dense>
#include "election.hpp"


// reads pivot table from csv file and stores data 
// void loadCSV(Eigen::MatrixXi &data, std::vector<std::string> &parties, std::vector<district> &districts, const std::string path, const char delim = ';') {
    

// }

// void loadCSV(election &e) {

// }

// void applyMinQuorum(const Eigen::MatrixXi &data) {

//     // at least 3% in total
//     for(int j = 0; j < data.cols())
    
    
//     // 5% in at least 1 district


// }

int main(int argc, char** argv) {
    // import 
    // Eigen::MatrixXi data;
    // std::vector<std::string> parties;
    // std::vector<district> districts;
    // loadCSV(data, parties, districts, "../data/2019_KRZH.csv", ';');

    election krzh19("../data/2019_KRZH.csv", ';');
    // for(auto x : krzh19.parties_) {
    //     std::cout << x << std::endl;
    // }
    // std::cout << "\n\n\n" << std::endl;
    krzh19.applyMinQuorum();
    // std::cout << krzh19.numSeats() << "\n\n";
    // for(auto d : krzh19.districts()) {
    //     std::cout << d.name_ << " " << d.seats_ << std::endl;
    // }
    // min quorum


    // Oberzuteilung
    krzh19.oberzuteilung();

    // Unterzuteilung

}
