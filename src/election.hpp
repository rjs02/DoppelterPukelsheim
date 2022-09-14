#include <string>
#include <vector>
#include <Eigen/Dense>

class district {
    public:
    std::string name_;
    // int id_;
    // int population_;
    int seats_;
    district(const std::string name, const int seats) : name_(name), seats_(seats) {}
};

class election {
    public:
    // ctor
    election(const std::string path, const char delim = ';');
    
    void applyMinQuorum();


    std::string name_;
    std::vector<district> districts_;
    std::vector<std::string> parties_;
    Eigen::MatrixXi votes_;

    private:
    int totalVotes_;
    int numSeats_;
    int numDistricts_;
    int numParties_;
};
