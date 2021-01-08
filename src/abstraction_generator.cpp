#include "abstraction_generator.hpp"
#include "kmeans.hpp"

//
// SUIT ISOMORPHIC ABSTRACTION
//

SuitIsomorphAbstractionGenerator::SuitIsomorphAbstractionGenerator(
    std::ofstream &dump_to)
    : AbstractionGenerator(dump_to) {
    uint8_t num_cards1[1] = {2};
    assert(hand_indexer_init(1, num_cards1, &indexer[0]));
    uint8_t num_cards2[2] = {2, 3};
    assert(hand_indexer_init(2, num_cards2, &indexer[1]));
    uint8_t num_cards3[2] = {2, 4};
    assert(hand_indexer_init(2, num_cards3, &indexer[2]));
    uint8_t num_cards4[2] = {2, 5};
    assert(hand_indexer_init(2, num_cards4, &indexer[3]));
}

void SuitIsomorphAbstractionGenerator::generate(nbgen &rng, std::vector<histogram_c> &round_centers) {
  for (unsigned i = 0; i < 4; ++i)
    generate_round(i, rng,round_centers[i]);
}

// TODO datatype to small when round two or three would be used with this
// abstraction
void SuitIsomorphAbstractionGenerator::generate_round(int round, nbgen &rng, histogram_c &center) {
  unsigned nb_entries = indexer[round].round_size[round == 0 ? 0 : 1];
        dump_to->write(reinterpret_cast<const char *>(&round),
                       sizeof(round));
  dump_to->write(reinterpret_cast<const char *>(&nb_entries),
                 sizeof(nb_entries));

  for (unsigned i = 0; i < nb_entries; ++i) {
    dump_to->write(reinterpret_cast<const char *>(&i), sizeof(nb_entries));
  }
}

//
// MIXED ABSTRACTION
//

MixedAbstractionGenerator::MixedAbstractionGenerator(
    std::ofstream &dump_to, std::vector<AbstractionGenerator *> generators)
    : AbstractionGenerator(dump_to), generators(generators) {}

MixedAbstractionGenerator::~MixedAbstractionGenerator() {}

void MixedAbstractionGenerator::generate(nbgen &rng, std::vector<histogram_c> &round_centers) {
  for (unsigned i = 0; i < generators.size(); ++i) {
    auto start = ch::steady_clock::now();
    generators[i]->generate_round(i, rng,round_centers[i]);
    std::cout << "round " << i << " eval and clustering took: "
              << ch::duration_cast<ch::seconds>(ch::steady_clock::now() - start)
                     .count() << " sec." << std::endl;;
  }
}

void MixedAbstractionGenerator::generate_round(int round, nbgen &rng, histogram_c &center) {}

//
// POTENTIAL AWARE ABSTRACTION
//

PotentialAwareAbstractionGenerator::PotentialAwareAbstractionGenerator(
    std::ofstream &dump_to, int potentialround,
    std::vector<AbstractionGenerator *> generators)
    : AbstractionGenerator(dump_to), generators(generators),
      potentialround(potentialround) {
    uint8_t num_cards1[1] = {2};
    assert(hand_indexer_init(1, num_cards1, &indexer[0]));
    uint8_t num_cards2[2] = {2, 3};
    assert(hand_indexer_init(2, num_cards2, &indexer[1]));
    uint8_t num_cards3[2] = {2, 4};
    assert(hand_indexer_init(2, num_cards3, &indexer[2]));
    uint8_t num_cards4[2] = {2, 5};
    assert(hand_indexer_init(2, num_cards4, &indexer[3]));
}

PotentialAwareAbstractionGenerator::~PotentialAwareAbstractionGenerator() {}

// calculate rounds backwards to be able to cluster with data of a later round
void PotentialAwareAbstractionGenerator::generate(nbgen &rng, std::vector<histogram_c> &round_centers) {
  for (unsigned i = generators.size()-1; i >= 0 ; --i) {
    auto start = ch::steady_clock::now();
    if( potentialround == i ){

    }
    else if( potentialround == (i+1) ) {
      // save centers from clustering for next round
      generators[i]->generate_round(i, rng,round_centers[i]);
    }
    else{
      generators[i]->generate_round(i, rng,round_centers[i]);
    }
    std::cout << "round " << i << " eval and clustering took: "
              << ch::duration_cast<ch::seconds>(ch::steady_clock::now() - start)
                     .count() << " sec." << std::endl;;
  }
}

void PotentialAwareAbstractionGenerator::generate_round(int round, nbgen &rng, histogram_c &center) {
}
