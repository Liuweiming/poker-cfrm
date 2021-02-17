#ifndef CARD_ABSTRACTION_HPP
#define CARD_ABSTRACTION_HPP

#include <ecalc/ecalc.hpp>
#include <ecalc/handranks.hpp>
#include <ecalc/single_handlist.hpp>
#include <fstream>
#include <thread>
#include "definitions.hpp"
#include "kmeans.hpp"

extern "C" {
#include "hand_index.h"
}

using std::string;

class CardAbstraction {
 public:
  virtual unsigned get_nb_buckets(const Game *game, int round) = 0;
  virtual int map_hand_to_bucket(card_c hand, card_c board, int round) = 0;
};

class NullCardAbstraction : public CardAbstraction {
  const int deck_size;
  const Game *game;
  std::vector<unsigned> nb_buckets;

 public:
  NullCardAbstraction(const Game *game, string param)
      : game(game),
        deck_size(game->numSuits * game->numRanks),
        nb_buckets(game->numRounds) {
    nb_buckets[0] = 1;
    for (int i = 0; i < game->numHoleCards; ++i) {
      nb_buckets[0] *= deck_size;
    }

    for (int r = 0; r < game->numRounds; ++r) {
      if (r > 0) {
        nb_buckets[r] = nb_buckets[r - 1];
      }
      for (int i = 0; i < game->numBoardCards[r]; ++i)
        nb_buckets[r] *= deck_size;
    }
  }

  virtual unsigned get_nb_buckets(const Game *game, int round) {
    return nb_buckets[round];
  }

  virtual int map_hand_to_bucket(card_c hand, card_c board, int round) {
    int bucket = 0;
    for (int i = 0; i < game->numHoleCards; ++i) {
      if (i > 0) {
        bucket *= deck_size;
      }
      uint8_t card = hand[i];
      bucket += rankOfCard(card) * game->numSuits + suitOfCard(card);
    }

    for (int r = 0; r <= round; ++r) {
      for (int i = bcStart(game, r); i < sumBoardCards(game, r); ++i) {
        bucket *= deck_size;
        uint8_t card = board[i];
        bucket += rankOfCard(card) * game->numSuits + suitOfCard(card);
      }
    }
    return bucket;
  }
};

class BlindCardAbstraction : public CardAbstraction {
 public:
  BlindCardAbstraction(const Game *game, string param) {}
  virtual unsigned get_nb_buckets(const Game *game, int round) { return 1; }
  virtual int map_hand_to_bucket(card_c hand, card_c board, int round) {
    return 0;
  }
};

class ClusterCardAbstraction : public CardAbstraction {
  struct hand_feature {
    double value;
    unsigned cluster;
  };
  std::vector<ecalc::ECalc *> calc;
  hand_indexer_t indexer[4];

 public:
  int_c nb_buckets;
  std::vector<std::vector<unsigned>> buckets;

  ClusterCardAbstraction() {}

  ClusterCardAbstraction(const Game *game, string param) {
    init(game->numRounds, param);
  }

  void init(int nb_rounds, string load_from) {
    this->nb_buckets = int_c(nb_rounds);
    this->buckets = std::vector<std::vector<unsigned>>(nb_rounds);
    uint8_t num_cards1[1] = {2};
    assert(hand_indexer_init(1, num_cards1, &indexer[0]));
    uint8_t num_cards2[2] = {2, 3};
    assert(hand_indexer_init(2, num_cards2, &indexer[1]));
    uint8_t num_cards3[2] = {2, 4};
    assert(hand_indexer_init(2, num_cards3, &indexer[2]));
    uint8_t num_cards4[2] = {2, 5};
    assert(hand_indexer_init(2, num_cards4, &indexer[3]));

    std::ifstream file(load_from.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open()) {
      std::cerr << "could not load abstraction from" << load_from << std::endl;
      exit(-1);
    }
    int round;
    for (size_t i = 0; i < nb_rounds; ++i) {
      file.read(reinterpret_cast<char *>(&round), sizeof(round));
      file.read(reinterpret_cast<char *>(&nb_buckets[round]),
                sizeof(nb_buckets[round]));
      buckets[i] =
          std::vector<unsigned>(indexer[round].round_size[round == 0 ? 0 : 1]);
      for (unsigned j = 0; j < buckets[round].size(); ++j) {
        file.read(reinterpret_cast<char *>(&buckets[round][j]),
                  sizeof(buckets[round][j]));
        if (!file.good()) {
          std::cerr << "read abstraction from" << load_from << "error"
                    << std::endl;
        }
      }
    }
    std::cout << "load finished" << std::endl;
  }

  ~ClusterCardAbstraction() {}

  virtual unsigned get_nb_buckets(const Game *game, int round) {
    return nb_buckets[round];
  }

  virtual int map_hand_to_bucket(card_c hand, card_c board, int round) {
    uint8_t cards[7];
    for (unsigned i = 0; i < hand.size(); ++i) cards[i] = hand[i];
    for (unsigned i = 2; i < board.size() + 2; ++i) cards[i] = board[i - 2];

    hand_index_t index = hand_index_last(&indexer[round], cards);
    // uint8_t unicards[7];
    // hand_unindex(&indexer[round], round == 0 ? 0 : 1, index, unicards);
    // char card_str[100];
    // printCards(hand.size() + board.size(), &(cards[0]), 100, card_str);
    // std::cout << card_str << ":";
    // printCards(hand.size() + board.size(), &(unicards[0]), 100, card_str);
    // std::cout << card_str << " " << std::endl;
    // std::cout << "round " << round << " index " << index << " bucket " << buckets[round][index] << std::endl;
    return buckets[round][index];
  }
};

#endif
