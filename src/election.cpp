// #include "election.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include "election.hpp"

#define MAX_ITER 10000
const double INFTY = 1e10;

// ctor, takes path to pivot table in csv format (and delim char), stores data in class members
election::election(const std::string path, const char delim) {
    using namespace Eigen;

    std::ifstream in(path);
    std::vector<int> values; // will be converted to data matrix
    std::string line, cell;
    std::string quorum_msg; // for logger

    std::getline(in, line);
    std::stringstream lineStream(line);
    std::getline(lineStream, cell, delim); // title (first cell)
    name_ = cell;

    // construct logger
    std::string name = "Protokoll_" + name_ + ".txt";
    logger_ = new std::ofstream(name);

    std::getline(lineStream, cell, delim); // read which min quora are to be applied
    switch (stoi(cell)) {
        case 0:
            quorum_ = quorum::none;
            quorum_msg = "No minimum quorum";
            break;
        case 1:
            quorum_ = quorum::local;
            quorum_msg = "At least 5% of the votes in one district";
            break;
        case 2:
            quorum_ = quorum::total;
            quorum_msg = "At least 3% of the votes in total";
            break;
        case 3:
            quorum_ = quorum::both;
            quorum_msg = "At least 5% of the votes in one district OR 3% in total";
            break;
        default:
            *logger_ << "Invalid quorum value in file.\n";
            break;
    }
    
    // read in parties
    while(std::getline(lineStream, cell, delim)) {
        // parties_.push_back(cell);
        parties_.emplace_back(party(cell));
    }
    numParties_ = parties_.size();

    // read in next lines (votes)
    unsigned rows = 0;
    numSeats_ = 0;
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
    std::string s = parties_[parties_.size()-1].name_;
    if (!s.empty() && (s[s.length()-1] == '\n' || s[s.length()-1] == (char)13)) {
        s.erase(s.length()-1);
    }
    parties_[parties_.size()-1] = s;

    votes_ = Map<Matrix<int, Dynamic, Dynamic, RowMajor>>(values.data(), rows, values.size()/rows);
    numDistricts_ = districts_.size();
    totalVotes_ = votes_.sum();

    // init protocol
    *logger_ << "----------------------\n"
             << "Protokoll \n"
             << name_ << "\n"
             << "----------------------\n\n";
    
    *logger_ << "Reading from file: " << path << "\n";
    *logger_ << "Number of districts: " << numDistricts_ << "\n";
    *logger_ << "Number of parties: " << numParties_ << "\n";
    *logger_ << "Number of seats: " << numSeats_ << "\n";
    *logger_ << "Total votes: " << totalVotes_ << "\n";
    *logger_ << "Minimum quorum: " << quorum_msg << "\n\n";

}

//dtor
election::~election() {
    delete logger_;
}

// remove parties that do not meet the minimum quorum (3% in total or 5% in at least one district)
void election::applyMinQuorum(){
    std::vector<bool> minQuorum(numParties_, false);
    
    if(quorum_ & total) { // at least 3% in total
        for(int j = 0; j < numParties_; ++j) {
            minQuorum[j] = votes_.col(j).sum() >= 0.03 * totalVotes_;
        }
    }

    if(quorum_ & local) { // *OR* 5% in at least 1 district
        for(int i = 0; i < numDistricts_; ++i) {
            for(int j = 0; j < numParties_; ++j) {
                if(minQuorum[j]) continue;
                minQuorum[j] = minQuorum[j] || votes_(i, j) >= 0.05 * votes_.row(i).sum();
            }
        }
    }

    // delete parties that don't meet min quorum
    for(int j = 0; j < numParties_; ++j) {
        if(!minQuorum[j]) {
            // remove j-th col
            numParties_--; // update numParties_ (globally)
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

// determine the number of seats for each party; german because
void election::oberzuteilung() {
    *logger_ << "Oberzuteilung\n";
    Eigen::MatrixXd waehlerzahlen = votes_.cast<double>();
    double wahlschluessel;

    // iterate over districts, divide votes by number of seats
    for(int i = 0; i < numDistricts_; ++i) {
        waehlerzahlen.row(i) /= districts_[i].seats_;
    }

    Eigen::VectorXd waehlerzahlen_partei = waehlerzahlen.colwise().sum();
    wahlschluessel = waehlerzahlen_partei.sum() / numSeats_;
    *logger_ << "Provisorischer Wahlschlüssel: " << wahlschluessel << "\n\n";
    Eigen::ArrayXd seats_party = (waehlerzahlen_partei.array() / wahlschluessel).round();

    // change wahlschluessel if rounded seats don't add up to numSeats_
    int i = 0;
    for(; seats_party.sum() != numSeats_ && i < MAX_ITER; ++i) { // max MAX_ITER iterations      
        // too many seats => increase wahlschluessel
        if(seats_party.sum() > numSeats_) ++wahlschluessel;

        // too few seats => decrease wahlschluessel
        else if(seats_party.sum() < numSeats_) --wahlschluessel;

        // recalculate seats
        seats_party = (waehlerzahlen_partei.array() / wahlschluessel);
        seats_party = seats_party.round();
    }


    // finished, save and output
    for(int j = 0; j < numParties_; ++j) {
        parties_[j].seats_ = seats_party(j);
        *logger_ << parties_[j].name_ << " ("  << parties_[j].seats_ << ")    ";
    }

    *logger_ << "\nDefinitiver Wahlschlüssel: " << wahlschluessel << std::endl;
    *logger_ << "Total seats: " << seats_party.sum() << std::endl;
    *logger_ << "Oberzuteilung abgeschlossen\n---------------------------\n\n";
}

// returns true if procedure is done, every seat is assinged properly
bool election::finished() {
    bool party = true, district = true, total = false;
    // check total seat count
    total = seats_.sum() == numSeats_;

    // check if every district has the right number of seats
    for(int i = 0; i < numDistricts_; ++i) {
        district = district && districts_[i].seats_ == seats_.row(i).sum();
        if(districts_[i].seats_ != seats_.row(i).sum()) 
            *logger_ << "District " << i << " has " << seats_.row(i).sum() << " seats, should have " << districts_[i].seats_ << "\n";
    }

    // check if every party has the right number of seats
    for(int j = 0; j < numParties_; ++j) {
        party = party && parties_[j].seats_ == seats_.col(j).sum();
        if(parties_[j].seats_ != seats_.col(j).sum()) 
            *logger_ << "Party " << j << " has " << seats_.col(j).sum() << " seats, should have " << parties_[j].seats_ << "\n";
    }

    bool result = party && district && total;
    // *logger_ << "--- finished? ---\n"
    //          << "result: " << result 
    //          << "\n-----------------\n";
    if(district) *logger_ << "districts ok\n";
    if(party) *logger_ << "parties ok\n";

    return result;
}

// increase Wahlkreisdivisor in case rounding yields too many seats
// double election::incrWKdivisor(int i, Eigen::ArrayXd &parteidivisor) {
    // idea: function returns new WKdivisor for which rounding yields one less total seat than before
    // formula: result = 0.5 * (wkd_1[0] + min{wkd_1[1], wkd_2[0]}) where wkd_1 is an array of the resp. wahlkreisdivisors for a 
    // change of 1 seat in total and wkd_2 is the same for a change of 2 seats in total

    // Eigen::ArrayXd wkdiv = wahlkreisdivisor;
    // std::sort(wkdiv.begin(), wkdiv.end());
    
    // calculate divisor for 1 seat change
    // Eigen::ArrayXd wkd_1 = votes_.row(i).cast<double>() / parteidivisor.transpose() / 

    
    
// }


// ben
void election::unterzuteilung() {
    *logger_ << "\n+----------------------+\n"
             <<   "|    Unterzuteilung    |\n"
             <<   "+----------------------+\n\n";

    constexpr double step = 0.0001; // parteidivisor step size
    
    Eigen::ArrayXd wahlkreisdivisor = Eigen::ArrayXd::Constant(numDistricts_, 1.0);
    Eigen::ArrayXd parteidivisor = Eigen::ArrayXd::Constant(numParties_, 1.0);
    seats_ = Eigen::MatrixXi::Zero(numDistricts_, numParties_);

    int iter = 0; // iteration counter
    for(; !finished() && iter < 20; ++iter) { // TODO: CHANGE BACK TO MAX_ITER
        *logger_ << "\n\nIteration " << iter << "\n";
        Eigen::ArrayXd sitze_unger;

        int i = 0; // district index
        for(; i < numDistricts_; ++i) {
            wahlkreisdivisor(i) = votes_.row(i).cast<double>().sum() / districts_[i].seats_;
            sitze_unger = (votes_.row(i).cast<double>().array() / parteidivisor.transpose() / wahlkreisdivisor(i)).array(); // wtf is dis lol
            *logger_ << "WK " << i << " sitze_unger: " << sitze_unger.transpose() << "\n";
            seats_.row(i) = sitze_unger.round().cast<int>(); 
            
            int iter_district = 0; // district iteration counter
            for(; seats_.row(i).sum() != districts_[i].seats_ && iter_district < MAX_ITER; ++iter_district) {
                if(seats_.row(i).sum() == districts_[i].seats_) { // rounding worked, number agrees with numSeats_
                    break;
                }
                else if(seats_.row(i).sum() < districts_[i].seats_) { // need to decrease wahlkreisdivisor
                    // wahlkreisdivisor(i) -= 2;
                    // calculate divisors for 1 and 2 seat change
                    Eigen::ArrayXd wkd1 = votes_.row(i).cast<double>().array() / parteidivisor.transpose() / (seats_.row(i).cast<double>().array() + 0.5);
                    Eigen::ArrayXd wkd2 = votes_.row(i).cast<double>().array() / parteidivisor.transpose() / (seats_.row(i).cast<double>().array() + 1.5);
                    std::sort(wkd1.begin(), wkd1.end(), std::greater<double>()); 
                    std::sort(wkd2.begin(), wkd2.end(), std::greater<double>());

                    wahlkreisdivisor(i) = 0.5 * (wkd1(0) + std::max(wkd1(1), wkd2(0)));
                    // wahlkreisdivisor(i) = 0.5 * (wkd1(0) + wkd1(1));
                }
                else if(seats_.row(i).sum() > districts_[i].seats_) { // need to increase wahlkreisdivisor
                    // ++wahlkreisdivisor(i) += 2;
                    // calculate divisors for 1 and 2 seat change
                    Eigen::ArrayXd wkd1 = votes_.row(i).cast<double>().array() / parteidivisor.transpose() / (seats_.row(i).cast<double>().array() - 0.5);
                    Eigen::ArrayXd wkd2 = votes_.row(i).cast<double>().array() / parteidivisor.transpose() / (seats_.row(i).cast<double>().array() - 1.5);
                    // TODO: change negative values in wkd1, wkd2 to +inf
                    wkd1 = (wkd1 < 0).select(INFTY, wkd1);
                    wkd2 = (wkd2 < 0).select(INFTY, wkd2);
                    std::sort(wkd1.begin(), wkd1.end());
                    std::sort(wkd2.begin(), wkd2.end());

                    wahlkreisdivisor(i) = 0.5 * (wkd1(0) + std::min(wkd1(1), wkd2(0)));
                    // wahlkreisdivisor(i) = 0.5 * (wkd1(0) + wkd1(1));
                }

                // recalculate seats
                // seats_.row(i) = (votes_.cast<double>().row(i) / wahlkreisdivisor(i)).array().round().cast<int>();
                // seats_.row(i) = (votes_.row(i).cast<double>().array() / parteidivisor.transpose() / wahlkreisdivisor(i)).array().round().cast<int>();
                sitze_unger = (votes_.row(i).cast<double>().array() / parteidivisor.transpose() / wahlkreisdivisor(i)).array(); // wtf is dis lol
                *logger_ << "WK " << i << " sitze_unger: " << sitze_unger.transpose() << "\n";
                seats_.row(i) = sitze_unger.round().cast<int>(); 
            }
            *logger_ << "District " << i << " finished after " << iter_district << " iterations\n";
        }
        *logger_ << "\n\nWahlkreisdivisor: " << wahlkreisdivisor.transpose() << "\n\n";
        *logger_ << seats_ << std::endl;

        if(finished()) break;

        int j = 0; // party index
        for(; j < numParties_; ++j) {
            int iter_party = 0; // party iteration counter
            for(; seats_.col(j).sum() != parties_[j].seats_ && iter_party < MAX_ITER; ++iter_party) {
                if(seats_.col(j).sum() == parties_[j].seats_) { // rounding worked, number agrees with numSeats_
                    break;
                }
                else if(seats_.col(j).sum() < parties_[j].seats_) { // need to decrease parteidivisor
                    // parteidivisor(j) -= step;
                    // calculate divisors for 1 and 2 seat change
                    Eigen::ArrayXd pd1 = votes_.col(j).cast<double>().array() / (seats_.col(j).cast<double>().array() + 0.5) / wahlkreisdivisor;
                    Eigen::ArrayXd pd2 = votes_.col(j).cast<double>().array() / (seats_.col(j).cast<double>().array() + 1.5) / wahlkreisdivisor;
                    std::sort(pd1.begin(), pd1.end(), std::greater<double>());
                    std::sort(pd2.begin(), pd2.end(), std::greater<double>());

                    parteidivisor(j) = 0.5 * (pd1(0) + std::max(pd1(1), pd2(0)));
                    // parteidivisor(j) = 0.5 * (pd1(0) + pd1(1));
                }
                else if(seats_.col(j).sum() > parties_[j].seats_) { // need to increase parteidivisor
                    // parteidivisor(j) += step;
                    // calculate divisors for 1 and 2 seat change
                    Eigen::ArrayXd pd1 = votes_.col(j).cast<double>().array() / (seats_.col(j).cast<double>().array() - 0.5) / wahlkreisdivisor;
                    Eigen::ArrayXd pd2 = votes_.col(j).cast<double>().array() / (seats_.col(j).cast<double>().array() - 1.5) / wahlkreisdivisor;
                    // TODO: change negative values to +inf
                    pd1 = (pd1 < 0).select(INFTY, pd1);
                    pd2 = (pd2 < 0).select(INFTY, pd2);
                    std::sort(pd1.begin(), pd1.end());
                    std::sort(pd2.begin(), pd2.end());

                    parteidivisor(j) = 0.5 * (pd1(0) + std::min(pd1(1), pd2(0)));
                    // parteidivisor(j) = 0.5 * (pd1(0) + pd1(1));
                }

                // recalculate seats
                sitze_unger = (votes_.col(j).cast<double>().array() / wahlkreisdivisor / parteidivisor(j)).array();
                *logger_ << "Party " << j << " sitze_unger: " << sitze_unger.transpose() << "\n";
                seats_.col(j) = sitze_unger.round().cast<int>();
            }
            *logger_ << "Party " << j << " finished after " << iter_party << " iterations\n";
        }
        *logger_ << "\n\nParteidivisor: " << parteidivisor.transpose() << "\n\n";
        *logger_ << seats_ << std::endl;

    }
    *logger_ << "\nouter loop took " << iter << " iterations\n";
    *logger_ << "\n\n\n" << seats_.transpose() << "\n\n" << seats_.sum() << std::endl;
    *logger_ << "district sums: " << seats_.rowwise().sum().transpose() << std::endl;
    *logger_ << "party sums: " << seats_.colwise().sum() << std::endl;
}