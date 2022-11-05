#include <iostream>
#include <string>
#include <vector>
#include <fstream>

#include <Eigen/Dense>
#include "election.hpp"
// #include "election.cpp"

int main(int argc, char** argv) {
    // import 
    // Eigen::MatrixXi data;
    // std::vector<std::string> parties;
    // std::vector<district> districts;
    // loadCSV(data, parties, districts, "../data/2019_KRZH.csv", ';');

    // election krzh19("../data/2019_KRZH.csv", ';');
    election krzh19("../data/example.csv", ';');
    krzh19.applyMinQuorum();
    krzh19.oberzuteilung();
    krzh19.unterzuteilung();

    // Unterzuteilung

}
