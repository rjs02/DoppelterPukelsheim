#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include "election.hpp"

constexpr int    election::maxIter_;
constexpr double election::infty_;

// ctor, takes path to pivot table in csv format (and delim char), stores data in class members
election::election(const std::string path, const char delim) {
    using namespace Eigen;

    std::ifstream in(path);
    std::vector<int> values; // will be converted to data matrix
    std::string line, cell;
    std::string quorum_msg;  // for logger

    std::getline(in, line);
    std::stringstream lineStream(line);
    std::getline(lineStream, cell, delim); // title (first cell)
    name_ = cell;

    // construct logger
    std::string file = "Protokoll_" + name_ + ".md";
    std::replace(file.begin(), file.end(), ' ', '_');
    logger_ = new std::ofstream(file);

    // read which min quora are to be applied
    std::getline(lineStream, cell, delim); 
    switch (stoi(cell)) {
        case 0:
            quorum_ = quorum::none;
            quorum_msg = "Kein Quorum.";
            break;
        case 1:
            quorum_ = quorum::local;
            quorum_msg = "Mindestens 5% der Stimmen in einem Wahlkreis.";
            break;
        case 2:
            quorum_ = quorum::total;
            quorum_msg = "Mindestens 3% der Stimmen insgesamt.";
            break;
        case 3:
            quorum_ = quorum::both;
            quorum_msg = "Mindestens 5% der Stimmen in einem Wahlkreis *oder* 3% der Stimmen insgesamt.";
            break;
        default:
            quorum_ = quorum::none; // default, although input is invalid
            *logger_ << "Ungültiges Quorum in Datei. Kein Quorum wird angewendet.";
            break;
    }
    
    // read in parties
    while(std::getline(lineStream, cell, delim)) {
        parties_.emplace_back(party(cell));
    }
    numParties_ = parties_.size();

    // read in next lines (votes)
    unsigned rows = 0;
    numSeats_ = 0;
    std::string dist, seats;
    while (std::getline(in, line)) {
        // read district name and seats
        std::stringstream lineStream(line);
        std::getline(lineStream, dist, delim);
        std::getline(lineStream, seats, delim);
        districts_.emplace_back(district(dist, std::stoi(seats))); 
        numSeats_ += std::stoi(seats);

        // read in votes
        while (std::getline(lineStream, cell, delim)) {
            values.push_back(std::stoi(cell));
        }
        ++rows;
    }
    
    // delete '\n' or CR (ASCII 13) at end of string of last party (some invisible character in the input file)
    std::string s = parties_[parties_.size()-1].name_;
    if (!s.empty() && (s[s.length()-1] == '\n' || s[s.length()-1] == (char)13)) {
        s.erase(s.length()-1);
    }
    parties_[parties_.size()-1] = s;

    // store to member variables
    votes_ = Map<Matrix<int, Dynamic, Dynamic, RowMajor>>(values.data(), rows, values.size()/rows);
    votes_original_ = votes_;
    numDistricts_ = districts_.size();
    totalVotes_ = votes_.sum();

    // init protocol
    *logger_ << "# " << name_ << " - Electoral Protocol\n"
             << "## Input\n"
             << "* Input aus Datei: `" << path << "`\n"
             << "* Anzahl Parteien: " << numParties_ << "\n"
             << "* Anzahl Wahlkreise: " << numDistricts_ << "\n"
             << "* Anzahl Ratssitze: " << numSeats_ << "\n"
             << "* Anzahl Parteistimmen: " << totalVotes_ << "\n"
             << "* Mindestquorum: " << quorum_msg << "\n\n";

}

//dtor
election::~election() {
    delete logger_;
}

// remove parties that do not meet the minimum quorum (3% in total or 5% in at least one district)
void election::applyMinQuorum(){
    *logger_ << "## Mindestquorum\n";
    std::vector<bool> minQuorum(numParties_, false); // true if party meets specified quorum
    std::vector<bool> localQuorum(numParties_, false);

    if(quorum_ & total) { // at least 3% in total
        *logger_ << "* Folgende Parteien erreichen mind. 3% der Gesamtstimmen: ";
        for(int j = 0; j < numParties_; ++j) {
            minQuorum[j] = votes_.col(j).sum() >= 0.03 * totalVotes_;
            if(minQuorum[j]) *logger_ << parties_[j].name_ << ", ";
        }
        *logger_ << "\n";
    }

    if(quorum_ & local) { // *OR* 5% in at least 1 district
        *logger_ << "* Folgende Parteien erreichen mind. 5% der Stimmen in mind. einem Wahlkreis: ";
        for(int i = 0; i < numDistricts_; ++i) {
            for(int j = 0; j < numParties_; ++j) {
                localQuorum[j] = localQuorum[j] || votes_(i,j) >= 0.05 * votes_.row(i).sum();
                minQuorum[j] = minQuorum[j] || localQuorum[j];
            }
        }
        for(int j = 0; j < numParties_; ++j) if(minQuorum[j]) *logger_ << parties_[j].name_ << ", ";
        *logger_ << "\n";
    }

    // delete parties that don't meet min quorum
    // has effect on votes_ and parties_
    for(int j = 0; j < numParties_ && quorum_ > 0; ++j) { // dont remove anything if quorum_ == 0
        if(!minQuorum[j]) {
            // remove j-th col
            numParties_--; // update numParties_ (globally)
            if(j < numParties_)
                votes_.block(0, j, numDistricts_, numParties_ - j) = votes_.rightCols(numParties_ - j);
            votes_.conservativeResize(numDistricts_, numParties_);
            parties_.erase(parties_.begin() + j);
            minQuorum.erase(minQuorum.begin() + j);
            --j; // check same index again since everything shifted left!
        }
    }

    // update total votes since we removed some parties
    totalVotes_ = votes_.sum();

    *logger_ << "* Neue Anzahl Parteien: " << numParties_ << "\n"
             << "* Neue Anzahl Parteistimmen: " << totalVotes_ << "\n\n";
}

// determine the number of seats for each party
// stores the result in the party.seats_ member
void election::oberzuteilung() {
    *logger_ << "## Oberzuteilung\n";
    Eigen::MatrixXd waehlerzahlen = votes_.cast<double>();
    double wahlschluessel = 1.0;

    // iterate over districts, divide votes by number of seats
    // to weaken the effect of large districts
    for(int i = 0; i < numDistricts_; ++i) {
        waehlerzahlen.row(i) /= districts_[i].seats_;
    }

    Eigen::VectorXd waehlerzahlen_partei = waehlerzahlen.colwise().sum();
    wahlschluessel = waehlerzahlen_partei.sum() / numSeats_;
    *logger_ << "* Provisorischer Wahlschlüssel: " << wahlschluessel << "\n\n";
    Eigen::ArrayXd seats_party_unger = (waehlerzahlen_partei.array() / wahlschluessel);
    Eigen::ArrayXd seats_party = seats_party_unger.round();

    // change wahlschluessel if rounded seats don't add up to numSeats_
    int i = 0;
    for(; seats_party.sum() != numSeats_ && i < maxIter_; ++i) {    
        // too many seats => increase wahlschluessel
        if(seats_party.sum() > numSeats_) {
            Eigen::ArrayXd wd1 = waehlerzahlen_partei.array() / (seats_party - 0.5); // wahlschlüssel for -1 seat
            Eigen::ArrayXd wd2 = waehlerzahlen_partei.array() / (seats_party - 1.5); // wahlschlüssel for -2 seats
            wd1 = (wd1 < 0).select(infty_, wd1);
            wd2 = (wd2 < 0).select(infty_, wd2);
            std::sort(wd1.begin(), wd1.end());
            std::sort(wd2.begin(), wd2.end());
            wahlschluessel = 0.5 * (wd1(0) + std::min(wd1(1), wd2(0)));
        }

        // too few seats => decrease wahlschluessel
        else if(seats_party.sum() < numSeats_) {
            Eigen::ArrayXd wd1 = waehlerzahlen_partei.array() / (seats_party + 0.5); // wahlschlüssel for +1 seat
            Eigen::ArrayXd wd2 = waehlerzahlen_partei.array() / (seats_party + 1.5); // wahlschlüssel for +2 seats
            std::sort(wd1.begin(), wd1.end(), std::greater<double>());
            std::sort(wd2.begin(), wd2.end(), std::greater<double>());
            wahlschluessel = 0.5 * (wd1(0) + std::max(wd1(1), wd2(0)));
        }

        // recalculate seats
        seats_party_unger = (waehlerzahlen_partei.array() / wahlschluessel);
        seats_party = seats_party_unger.round();
    }


    // finished, save and output
    *logger_ << "* Definitiver Wahlschlüssel: " << wahlschluessel << "\n"
             << "* Konvergierte in " << i+1 << " Iterationen.\n\n"
             << "### Zugeteilte Sitze:\n  |  | ";
    for(int j = 0; j < numParties_; ++j) {
        parties_[j].seats_ = seats_party(j);
        *logger_ << parties_[j].name_ << " | ";
    }
    std::string sep = "";
    for(int j = 0; j <= numParties_; ++j) sep += ":---:|";
    *logger_ << "\n  |" << sep << "\n"
             << "  | gerundet | ";
    for(int j = 0; j < numParties_; ++j) {
        *logger_ << parties_[j].seats_ << " | ";
    }
    *logger_ << "\n  | ungerundet | ";
    for(int j = 0; j < numParties_; ++j) {
        *logger_ << std::round(1000 * seats_party_unger(j)) / 1000.0 << " | ";
    }
    *logger_ << "\n\n";
}

// returns true if procedure is done, every seat is assigned properly
bool election::finished() {
    // init
    bool party = true, district = true, total = false;
    
    // check total seat count
    total = seats_.sum() == numSeats_;

    // check if every district has the right number of seats (given from input file)
    for(int i = 0; i < numDistricts_; ++i) {
        district = district && districts_[i].seats_ == seats_.row(i).sum();
    }

    // check if every party has the right number of seats (determined by oberzuteilung)
    for(int j = 0; j < numParties_; ++j) {
        party = party && parties_[j].seats_ == seats_.col(j).sum();
    }

    return party && district && total;
}

// outputs result as markdown table
template <typename T>
void election::outputTable(Eigen::Matrix<T, -1, -1> &m, Eigen::ArrayXd &wkd, Eigen::ArrayXd &pd) {
    // if(!rounded) auto& m = seats_unger_;
    // else auto& m = seats_;

    std::string sep = "";
    for(int j = 0; j < numParties_+2; ++j) sep += ":---:|";
    sep = "|" + sep;

    *logger_ << "|  | ";
    for(int j = 0; j < numParties_; ++j) {
        *logger_ << parties_[j].name_ << " | ";
    }
    *logger_ << " Wahlkreisdivisor |\n"
             << sep << "\n| ";

    for(int i = 0; i < numDistricts_; ++i) {
        *logger_ << districts_[i].name_ << " | ";
        for(int j = 0; j < numParties_; ++j) {
            *logger_ << std::round(m(i,j) * 1000) / 1000.0 << " | ";
        }
        *logger_ << std::round(wkd(i) * 10) / 10.0 << " |\n";
    }
    *logger_ << "| Parteidivisor | ";
    for(int j = 0; j < numParties_; ++j) {
        *logger_ << std::round(pd(j) * 1000) / 1000.0 << " | ";
    }
    *logger_ << "|\n\n";
}


// assigns seats to parties per district
void election::unterzuteilung() {
    *logger_ << "## Unterzuteilung\n";

    // init
    Eigen::ArrayXd wahlkreisdivisor = Eigen::ArrayXd::Constant(numDistricts_, 1.0);
    Eigen::ArrayXd parteidivisor    = Eigen::ArrayXd::Constant(numParties_, 1.0);
    Eigen::ArrayXd sitze_unger      = Eigen::ArrayXd::Constant(numParties_, 0.0);
    seats_ = Eigen::MatrixXi::Zero(numDistricts_, numParties_);
    seats_unger_ = seats_.cast<double>();

    int iter = 1; // iteration counter
    for(; !finished() && iter < maxIter_; ++iter) {

        int i = 0; // district index
        for(; i < numDistricts_; ++i) {
            wahlkreisdivisor(i) = votes_.row(i).cast<double>().sum() / districts_[i].seats_;
            seats_unger_.row(i) = (votes_.row(i).cast<double>().array() / parteidivisor.transpose() / wahlkreisdivisor(i)).array(); // wtf is dis lol
            seats_.row(i) = sitze_unger.round().cast<int>(); 
            
            int iter_district = 0; // district iteration counter
            for(; seats_.row(i).sum() != districts_[i].seats_ && iter_district < maxIter_; ++iter_district) {
                
                if(seats_.row(i).sum() < districts_[i].seats_) { // need to decrease wahlkreisdivisor
                    // calculate divisors for 1 and 2 seat change
                    Eigen::ArrayXd wkd1 = votes_.row(i).cast<double>().array() / parteidivisor.transpose() / (seats_.row(i).cast<double>().array() + 0.5);
                    Eigen::ArrayXd wkd2 = votes_.row(i).cast<double>().array() / parteidivisor.transpose() / (seats_.row(i).cast<double>().array() + 1.5);
                    std::sort(wkd1.begin(), wkd1.end(), std::greater<double>()); 
                    std::sort(wkd2.begin(), wkd2.end(), std::greater<double>());
                    wahlkreisdivisor(i) = 0.5 * (wkd1(0) + std::max(wkd1(1), wkd2(0)));
                }

                else if(seats_.row(i).sum() > districts_[i].seats_) { // need to increase wahlkreisdivisor
                    // calculate divisors for 1 and 2 seat change
                    Eigen::ArrayXd wkd1 = votes_.row(i).cast<double>().array() / parteidivisor.transpose() / (seats_.row(i).cast<double>().array() - 0.5);
                    Eigen::ArrayXd wkd2 = votes_.row(i).cast<double>().array() / parteidivisor.transpose() / (seats_.row(i).cast<double>().array() - 1.5);
                    wkd1 = (wkd1 < 0).select(infty_, wkd1);
                    wkd2 = (wkd2 < 0).select(infty_, wkd2);
                    std::sort(wkd1.begin(), wkd1.end());
                    std::sort(wkd2.begin(), wkd2.end());
                    wahlkreisdivisor(i) = 0.5 * (wkd1(0) + std::min(wkd1(1), wkd2(0)));
                }

                // recalculate seats
                seats_unger_.row(i) = (votes_.row(i).cast<double>().array() / parteidivisor.transpose() / wahlkreisdivisor(i)).array(); // wtf is dis lol
                seats_.row(i) = seats_unger_.row(i).array().round().cast<int>(); 
            }
        }

        if(finished()) break;

        int j = 0; // party index
        for(; j < numParties_; ++j) {
            int iter_party = 0; // party iteration counter
            for(; seats_.col(j).sum() != parties_[j].seats_ && iter_party < maxIter_; ++iter_party) {

                if(seats_.col(j).sum() < parties_[j].seats_) { // need to decrease parteidivisor
                    // calculate divisors for 1 and 2 seat change
                    Eigen::ArrayXd pd1 = votes_.col(j).cast<double>().array() / (seats_.col(j).cast<double>().array() + 0.5) / wahlkreisdivisor;
                    Eigen::ArrayXd pd2 = votes_.col(j).cast<double>().array() / (seats_.col(j).cast<double>().array() + 1.5) / wahlkreisdivisor;
                    std::sort(pd1.begin(), pd1.end(), std::greater<double>());
                    std::sort(pd2.begin(), pd2.end(), std::greater<double>());
                    parteidivisor(j) = 0.5 * (pd1(0) + std::max(pd1(1), pd2(0)));
                }

                else if(seats_.col(j).sum() > parties_[j].seats_) { // need to increase parteidivisor
                    // calculate divisors for 1 and 2 seat change
                    Eigen::ArrayXd pd1 = votes_.col(j).cast<double>().array() / (seats_.col(j).cast<double>().array() - 0.5) / wahlkreisdivisor;
                    Eigen::ArrayXd pd2 = votes_.col(j).cast<double>().array() / (seats_.col(j).cast<double>().array() - 1.5) / wahlkreisdivisor;
                    pd1 = (pd1 < 0).select(infty_, pd1);
                    pd2 = (pd2 < 0).select(infty_, pd2);
                    std::sort(pd1.begin(), pd1.end());
                    std::sort(pd2.begin(), pd2.end());
                    parteidivisor(j) = 0.5 * (pd1(0) + std::min(pd1(1), pd2(0)));
                }

                // recalculate seats
                seats_unger_.col(j) = (votes_.col(j).cast<double>().array() / wahlkreisdivisor / parteidivisor(j)).array();
                seats_.col(j) = seats_unger_.col(j).array().round().cast<int>();
            }
        }
    }
    *logger_ << "Konvergierte in " << iter+1 << " Iterationen.\n\n";

    // output as markdown table
    *logger_ << "### Gerundet: \n";
    outputTable(seats_, wahlkreisdivisor, parteidivisor);
    *logger_ << "\n### Ungerundet: \n";
    outputTable(seats_unger_, wahlkreisdivisor, parteidivisor);
}

void election::exportResults() {
    std::ofstream out("data.out");
    // parties
    for(int j = 0; j < numParties_; ++j) out << parties_[j].name_ << ";";
    out << "\n";
    // districts
    for(int i = 0; i < numDistricts_; ++i) out << districts_[i].name_ << ";";
    out << "\n";
    // votes (row major)
    for(int i = 0; i < numDistricts_; ++i) {
        for(int j = 0; j < numParties_; ++j) {
            out << votes_(i, j) << ";";
        }
    }
    out << "\n";
    // seats (row major)
    for(int i = 0; i < numDistricts_; ++i) {
        for(int j = 0; j < numParties_; ++j) {
            out << seats_(i, j) << ";";
        }
    }
    out << "\n";
    // seats unrounded (row major)
    for(int i = 0; i < numDistricts_; ++i) {
        for(int j = 0; j < numParties_; ++j) {
            out << seats_unger_(i, j) << ";";
        }
    }
}
