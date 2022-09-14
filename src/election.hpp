#include <string>
#include <vector>
#include <Eigen/Dense>

class district {
    public:
    // ctor
    district(const std::string name, const int seats) : name_(name), seats_(seats) {}
    
    // int id_;
    // int population_;
    std::string name_;
    int seats_;
};

class election {
    public:
    // ctor
    election(const std::string path, const char delim = ';');
    
    void applyMinQuorum();
    void oberzuteilung();

    std::string name_;
    std::vector<district> districts_;
    std::vector<std::string> parties_;
    Eigen::MatrixXi votes_;

    // getters
    int numDistricts() const { return numDistricts_; }
    int numParties() const { return numParties_; }
    int numSeats() const { return numSeats_; }
    int totalVotes() const { return totalVotes_; }
    std::vector<district> districts() const { return districts_; }
    std::vector<std::string> parties() const { return parties_; }

    private:
    int totalVotes_;
    int numSeats_;
    int numDistricts_;
    int numParties_;
    std::vector<int> seatDistr_;
};
