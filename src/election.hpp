#include <string>
#include <vector>
#include <Eigen/Dense>
#include <fstream>

// #pragma once
#ifndef ELECTION_HPP
#define ELECTION_HPP

class district {
    public:
    // ctor
    district(const std::string name, const int seats) : name_(name), seats_(seats) {}

    // int id_;
    // int population_;
    std::string name_;
    int seats_; // assinged in input file
};

class party {
    public:
    // ctor
    party(const std::string name, const int seats = 0) : name_(name), seats_(seats) {}

    std::string name_;
    int seats_; // assinged by Oberzuteilung
};


class election {
    public:
    // ctor
    election(const std::string path, const char delim = ';');
    // dtor
    ~election();

    void applyMinQuorum();
    void oberzuteilung();
    void unterzuteilung();
    bool finished();

    std::string name_;
    std::vector<district> districts_;
    // std::vector<std::string> parties_;
    std::vector<party> parties_;
    Eigen::MatrixXi votes_;
    enum quorum : char { none = 0, local, total, both };

    // getters
    int numDistricts() const { return numDistricts_; }
    int numParties() const { return numParties_; }
    int numSeats() const { return numSeats_; }
    int totalVotes() const { return totalVotes_; }
    std::vector<district> districts() const { return districts_; }
    // std::vector<std::string> parties() const { return parties_; }
    std::vector<party> parties() const { return parties_; }
    // std::vector<int> seatDistribution() const { return seatDistr_; }

    private:
    int totalVotes_;    // calculated from input file
    int numSeats_;      // calculated from input file
    int numDistricts_;  // calculated from input file
    int numParties_;    // calculated from input file
    // std::vector<int> seatDistr_;
    Eigen::MatrixXi seats_; // seats for each party in each district
    std::ofstream *logger_; // output to log file ("Wahlprotokoll")
    quorum quorum_;

    // double incrWKdivisor();
    
};

#endif // ELECTION_HPP
