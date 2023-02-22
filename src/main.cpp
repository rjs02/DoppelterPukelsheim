#include "election.hpp"

int main(int argc, char** argv) {
    election e(argv[1], argv[2][0]);
    e.applyMinQuorum();
    e.oberzuteilung();
    e.unterzuteilung();
    e.exportResults();
}
