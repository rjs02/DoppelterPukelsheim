#include "election.hpp"

int main(int argc, char** argv) {
    election krzh19("../data/2019_KRZH.csv", ';');
    krzh19.applyMinQuorum();
    krzh19.oberzuteilung();
    krzh19.unterzuteilung();
    krzh19.exportResults();
}
