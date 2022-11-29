#include <string>
#include <vector>
#include <fstream>
#include <Eigen/Dense>


#ifndef ELECTION_HPP
#define ELECTION_HPP

struct district {
    // ctor
    district(const std::string name, const int seats = 0) : name_(name), seats_(seats) {}

    std::string name_;
    int seats_; // assinged in input file
};

struct party {
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

    void applyMinQuorum();                   // applies minimum quorum
    void oberzuteilung();                    // performs Oberzuteilung
    void unterzuteilung();                   // performs Unterzuteilung
    bool finished();                         // returns true if all seats are assigned correctly subject to constraints
    void exportResults();                    // exports results compactly to plot

    enum quorum : char { none = 0, local, total, both };

    // getters
    int numDistricts() const { return numDistricts_; }
    int numParties() const { return numParties_; }
    int numSeats() const { return numSeats_; }
    int totalVotes() const { return totalVotes_; }
    std::vector<district> districts() const { return districts_; }
    std::vector<party> parties() const { return parties_; }
    

    private:
    int totalVotes_;                         // calculated from input file
    int numSeats_;                           // calculated from input file
    int numDistricts_;                       // calculated from input file
    int numParties_;                         // calculated from input file

    Eigen::MatrixXi votes_;                  // votes per party and district (from input file)
    Eigen::MatrixXi votes_original_;         // votes matrix before applying quora (from input file, only used to output for analysis purposes)
    Eigen::MatrixXi seats_;                  // seats per party and district (wanted result of program)
    Eigen::MatrixXd seats_unger_;            // seats per party and district (intermediate result of program) [unrounded]

    std::vector<district> districts_;        // contains election districts
    std::vector<party> parties_;             // contains party names and seats

    std::ofstream *logger_;                  // output to log file ("Wahlprotokoll")
    std::string name_;                       // name of election (from input file)
    quorum quorum_;                          // quorum type

    static constexpr int    maxIter_ = 1000; // hard limit for loop iterations in case sth doesn't converge
    static constexpr double infty_   = 1e10; // used in ober- and unterzuteilung
    
    template <typename T>
    void outputTable(Eigen::Matrix<T, -1, -1> &m, Eigen::ArrayXd &wkd, Eigen::ArrayXd &pd);
};

#endif // ELECTION_HPP
