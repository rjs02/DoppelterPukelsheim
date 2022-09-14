#include "election.hpp"
#include <iostream>
#include <fstream>

// ctor, takes path to pivot table in csv format (and delim char), stores data in class members
election::election(const std::string path, const char delim) {
    using namespace Eigen;

    std::ifstream in(path);
    std::vector<int> values; // will be converted to data matrix
    std::string line, cell;

    std::getline(in, line);
    std::stringstream lineStream(line);
    std::getline(lineStream, cell, delim); // title (first cell)
    name_ = cell;
    std::getline(lineStream, cell, delim); // throw away cell "KR"
    
    // read in parties
    while(std::getline(lineStream, cell, delim)) {
        parties_.push_back(cell);
    }
    numParties_ = parties_.size();

    // read in next lines (votes)
    unsigned rows = 0;
    while (std::getline(in, line)) {
        std::stringstream lineStream(line);
        std::string dist, seats;
        std::getline(lineStream, dist, delim);
        std::getline(lineStream, seats, delim);
        districts_.emplace_back(district(dist, std::stoi(seats)));    // add first two entries two votes vector
        numSeats_ += std::stoi(seats);

        while (std::getline(lineStream, cell, delim)) {
            values.push_back(std::stoi(cell));
        }
        ++rows;
    }
    
    // delete '\n' or CR (ASCII 13) at end of string of last party
    std::string s = parties_[parties_.size()-1];
    if (!s.empty() && (s[s.length()-1] == '\n' || s[s.length()-1] == (char)13)) {
        s.erase(s.length()-1);
    }
    parties_[parties_.size()-1] = s;

    votes_ = Map<Matrix<int, Dynamic, Dynamic, RowMajor>>(values.data(), rows, values.size()/rows);
    numDistricts_ = districts_.size();
    totalVotes_ = votes_.sum();
}

// remove parties that do not meet the minimum quorum (3% in total or 5% in at least one district)
void election::applyMinQuorum(){
    std::vector<bool> minQuorum(numParties_, false);
    
    // at least 3% in total
    for(int j = 0; j < numParties_; ++j) {
        minQuorum[j] = votes_.col(j).sum() >= 0.03 * totalVotes_;
    }

    // or 5% in at least 1 district
    for(int i = 0; i < numDistricts_; ++i) {
        for(int j = 0; j < numParties_; ++j) {
            if(minQuorum[j]) continue;
            minQuorum[j] = minQuorum[j] || votes_(i, j) >= 0.05 * votes_.row(i).sum();
        }
    }

    // delete parties that don't meet min quorum
    for(int j = 0; j < numParties_; ++j) {
        if(!minQuorum[j]) {
            // remove j-th col
            numParties_--;
            if(j < numParties_)
                votes_.block(0, j, numDistricts_, numParties_ - j) = votes_.rightCols(numParties_ - j);
            votes_.conservativeResize(numDistricts_, numParties_);
            parties_.erase(parties_.begin() + j);
            minQuorum.erase(minQuorum.begin() + j);
            --j; // check same index again since everything shifted left
        }
    }

    // update total votes
    totalVotes_ = votes_.sum();
}
